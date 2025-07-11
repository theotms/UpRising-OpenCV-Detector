#include "ai_handler.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm> // For std::sort
#include <limits>    // For std::numeric_limits

// --- CONFIGURATION CONSTANTS ---
constexpr int ARENA_WIDTH = 480;
constexpr int ARENA_HEIGHT = 480;
constexpr int OBSERVATION_SIZE = 22;

// TODO: Set the coordinates of the opponent's goal.
// Assuming the top-down map is 480x480, the goal might be centered on one edge.
const cv::Point2f OPPONENT_GOAL_POSITION(ARENA_WIDTH / 2.0f, 0.0f); // Example: Centered on the top edge

AIHandler::AIHandler(const std::string& model_path)
    : env(ORT_LOGGING_LEVEL_WARNING, "RobotSoccerAI"),
      session(env, model_path.c_str(), Ort::SessionOptions{nullptr}) {
    std::cout << "[AI] ONNX model loaded successfully from: " << model_path << std::endl;
}

// This function now implements the logic you provided for the 22 parameters.
std::vector<float> AIHandler::createObservationVector(const Bot& current_bot, const WorldState& world) {
    std::vector<float> obs(OBSERVATION_SIZE, 0.0f);

    // --- 1. Agent's Own State (4 floats) ---
    // Using y-coordinate as the z-coordinate for the AI model
    obs[0] = current_bot.center.x / ARENA_WIDTH;  // Normalized X position
    obs[1] = current_bot.center.y / ARENA_HEIGHT; // Normalized Z position
    float angle_rad = current_bot.angle * CV_PI / 180.0;
    obs[2] = std::cos(angle_rad); // Facing X component
    obs[3] = std::sin(angle_rad); // Facing Z component

    // --- 2. Teammate's State (3 floats) ---
    const Bot* teammate = nullptr;
    float min_teammate_dist = std::numeric_limits<float>::max();
    for (const auto& other_bot : world.bots) {
        if (other_bot.id == current_bot.id) continue; // Skip self
        // TODO: Add logic here to distinguish teammates from opponents if necessary.
        // For now, we assume any other bot is a potential teammate.
        float dist = cv::norm(current_bot.center - other_bot.center);
        if (dist < min_teammate_dist) {
            min_teammate_dist = dist;
            teammate = &other_bot;
        }
    }

    if (teammate) {
        cv::Point2f dir_to_teammate = teammate->center - current_bot.center;
        float dist_to_teammate = cv::norm(dir_to_teammate);
        obs[4] = dist_to_teammate / ARENA_WIDTH; // Normalized distance
        if (dist_to_teammate > 1e-6) {
            obs[5] = dir_to_teammate.x / dist_to_teammate; // Normalized direction X
            obs[6] = dir_to_teammate.y / dist_to_teammate; // Normalized direction Z
        }
    }

    // --- 3. The Goal's State (3 floats) ---
    cv::Point2f dir_to_goal = OPPONENT_GOAL_POSITION - current_bot.center;
    float dist_to_goal = cv::norm(dir_to_goal);
    obs[7] = dist_to_goal / ARENA_WIDTH; // Normalized distance
    if (dist_to_goal > 1e-6) {
        obs[8] = dir_to_goal.x / dist_to_goal; // Normalized direction X
        obs[9] = dir_to_goal.y / dist_to_goal; // Normalized direction Z
    }

    // --- 4. The Four Closest Balls' State (12 floats) ---
    std::vector<Ball> sorted_balls = world.balls;
    std::sort(sorted_balls.begin(), sorted_balls.end(), [&](const Ball& a, const Ball& b) {
        return cv::norm(a.center - current_bot.center) < cv::norm(b.center - current_bot.center);
    });

    for (int i = 0; i < 4; ++i) {
        int base_idx = 10 + i * 3;
        if (i < sorted_balls.size()) {
            const Ball& ball = sorted_balls[i];
            cv::Point2f dir_to_ball = ball.center - current_bot.center;
            float dist_to_ball = cv::norm(dir_to_ball);
            obs[base_idx] = dist_to_ball / ARENA_WIDTH; // Normalized distance
            if (dist_to_ball > 1e-6) {
                obs[base_idx + 1] = dir_to_ball.x / dist_to_ball; // Normalized direction X
                obs[base_idx + 2] = dir_to_ball.y / dist_to_ball; // Normalized direction Z
            }
        }
        // If fewer than 4 balls are detected, the remaining features will stay 0.0f
    }

    return obs;
}

std::map<int, MovementCommand> AIHandler::predictMovements(const WorldState& world) {
    std::map<int, MovementCommand> commands;
    if (world.bots.empty()) {
        return commands;
    }

    std::vector<float> obs_0_data;
    std::vector<float> action_masks_data;
    std::vector<const Bot*> bot_order;

    for (const auto& bot : world.bots) {
        std::vector<float> single_obs = createObservationVector(bot, world);
        obs_0_data.insert(obs_0_data.end(), single_obs.begin(), single_obs.end());
        action_masks_data.push_back(1.0f); // Default action mask
        bot_order.push_back(&bot);
    }

    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

    std::vector<int64_t> obs_0_shape = {static_cast<int64_t>(world.bots.size()), OBSERVATION_SIZE};
    Ort::Value obs_0_tensor = Ort::Value::CreateTensor<float>(memory_info, obs_0_data.data(), obs_0_data.size(), obs_0_shape.data(), obs_0_shape.size());

    std::vector<int64_t> action_masks_shape = {static_cast<int64_t>(world.bots.size()), 1};
    Ort::Value action_masks_tensor = Ort::Value::CreateTensor<float>(memory_info, action_masks_data.data(), action_masks_data.size(), action_masks_shape.data(), action_masks_shape.size());

    const char* input_names[] = {"obs_0", "action_masks"};
    const char* output_names[] = {"continuous_actions"}; // We only need this one output

    std::vector<Ort::Value> input_tensors;
    input_tensors.push_back(std::move(obs_0_tensor));
    input_tensors.push_back(std::move(action_masks_tensor));

    try {
        auto output_tensors = session.Run(Ort::RunOptions{nullptr}, input_names, input_tensors.data(), input_tensors.size(), output_names, 1);

        const float* actions_data = output_tensors[0].GetTensorData<float>();

        for (size_t i = 0; i < bot_order.size(); ++i) {
            int bot_id = bot_order[i]->id;
            float forward_cmd = actions_data[i * 2];
            float steer_cmd = actions_data[i * 2 + 1];

            // Map forward/steer to left/right motor commands
            float left_motor = forward_cmd - steer_cmd;
            float right_motor = forward_cmd + steer_cmd;

            // Clamp the values to the valid range of [-1.0, 1.0]
            left_motor = std::clamp(left_motor, -1.0f, 1.0f);
            right_motor = std::clamp(right_motor, -1.0f, 1.0f);

            commands[bot_id] = {left_motor, right_motor};
        }
    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime inference error: " << e.what() << std::endl;
    }

    return commands;
}