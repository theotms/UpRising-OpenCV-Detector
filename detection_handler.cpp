#include "detection_handler.h"
#include "arena_detector.h"
#include "bot_detector.h"
#include "ball_detector.h"
#include "ai_handler.h" // <-- Include the new AI handler
#include <opencv2/opencv.hpp>
#include <thread>
#include "mqtt_publisher.h"
#include "json.hpp"
#include "world_state.h"

using json = nlohmann::json;
using namespace cv;
using namespace std;

void detectionLoop(const Mat& cameraMatrix, const Mat& distCoeffs,
                   float markerLength, SharedState& state) {

    MQTTPublisher mqtt("tcp://192.168.0.122:1883", "robots/commands");
    mqtt.connect();

    // --- *** NEW: Initialize the AI Handler *** ---
    // It will look for the model file in the same directory as the executable.
    AIHandler ai_handler("RobotSoccerTeamA.onnx");

    auto lastUpdate = chrono::steady_clock::now();

    while (state.running) {
        Mat frame;
        {
            lock_guard<mutex> lock(state.frameMutex);
            if (state.sharedFrame.empty()) continue;
            state.sharedFrame.copyTo(frame);
        }

        Mat displayFrame = frame.clone();

        // --- Detection Logic (Unchanged) ---
        vector<Ball> currentBalls = detectOrangeBalls(frame);
        Mat current_H = detectArenaMarkers(frame, displayFrame, cameraMatrix, distCoeffs, markerLength, currentBalls);
        if (!current_H.empty()) { lock_guard<mutex> lock(state.dataMutex); state.last_known_H = current_H; }
        vector<DetectedBot> current_bots = detectBots(frame, displayFrame, cameraMatrix, distCoeffs, markerLength);
        if (!current_bots.empty()) { lock_guard<mutex> lock(state.dataMutex); state.last_known_bots = current_bots; }

        // --- AI Prediction & Publishing Logic ---
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastUpdate).count();

        if (elapsed >= 100) {
            WorldState world;
            // ... (Code to build the 'world' object is the same as before) ...
            {
                Mat H_for_transform;
                vector<DetectedBot> bots_to_transform;
                {
                    lock_guard<mutex> lock(state.dataMutex);
                    H_for_transform = state.last_known_H;
                    bots_to_transform = state.last_known_bots;
                }
                if (!H_for_transform.empty()) {
                    if (!bots_to_transform.empty()) {
                        vector<Point2f> bot_centers_in, bot_centers_out;
                        for(const auto& bot : bots_to_transform) { bot_centers_in.push_back(bot.center); }
                        perspectiveTransform(bot_centers_in, bot_centers_out, H_for_transform);
                        for (size_t i = 0; i < bots_to_transform.size(); ++i) {
                            world.bots.push_back({bots_to_transform[i].id, bot_centers_out[i], bots_to_transform[i].angleDeg, bots_to_transform[i].isAI});
                        }
                    }
                    if (!currentBalls.empty()) {
                        vector<Point2f> ball_in = {currentBalls.front().center}, ball_out;
                        perspectiveTransform(ball_in, ball_out, H_for_transform);
                        world.ball.center = ball_out[0];
                        world.ball.radius = currentBalls.front().radius;
                    }
                }
            }

            // --- *** NEW: Get movement commands from the real AI *** ---
            auto commands = ai_handler.predictMovements(world);

            // --- Build and publish the command JSON (Same as before) ---
            json command_list = json::array();
            for (const auto& [id, cmd] : commands) {
                json single_command;
                single_command["id"] = id;
                single_command["left"] = cmd.left;
                single_command["right"] = cmd.right;
                command_list.push_back(single_command);
            }
            if (!command_list.empty()) {
                json command_payload;
                command_payload["commands"] = command_list;
                std::cout << "Publishing AI Commands: " << command_payload.dump() << std::endl;
                mqtt.publish(command_payload.dump());
            }

            lastUpdate = now;

            // --- Top Down View Drawing (Unchanged) ---
            Mat topDownMap = Mat::zeros(480, 480, CV_8UC3);
            // ... (all the circle and line drawing code is the same) ...
            circle(topDownMap, Point(0, 0), 5, Scalar(255, 255, 255), -1);
            circle(topDownMap, Point(480, 0), 5, Scalar(255, 255, 255), -1);
            circle(topDownMap, Point(0, 480), 5, Scalar(255, 255, 255), -1);
            circle(topDownMap, Point(480, 480), 5, Scalar(255, 255, 255), -1);
            if (world.ball.radius > 0) { circle(topDownMap, world.ball.center, 10, Scalar(0, 165, 255), -1); }
            for (const auto& bot : world.bots) {
                RotatedRect botRect(bot.center, Size2f(30, 25), bot.angle);
                Point2f vertices[4];
                botRect.points(vertices);
                for (int i = 0; i < 4; i++) { line(topDownMap, vertices[i], vertices[(i + 1) % 4], Scalar(255, 0, 0), 2); }
                Point2f front_point(bot.center.x + 15 * cos(bot.angle * CV_PI / 180.0), bot.center.y + 15 * sin(bot.angle * CV_PI / 180.0));
                line(topDownMap, bot.center, front_point, Scalar(0, 255, 0), 2);
            }
            imshow("Top Down View", topDownMap);
        }

        imshow("Arena View", displayFrame);
        if (waitKey(1) == 'q') { state.running = false; break; }
    }
}

void captureLoop(VideoCapture& cap, SharedState& state) {
    Mat local;
    while (state.running) {
        cap >> local;
        if (local.empty()) continue;
        {
            lock_guard<mutex> lock(state.frameMutex);
            local.copyTo(state.sharedFrame);
        }
        this_thread::sleep_for(chrono::milliseconds(1));
    }
}