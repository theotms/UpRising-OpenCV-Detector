#include "detection_handler.h"
#include "arena_detector.h"
#include "bot_detector.h"
#include "ball_detector.h"
#include "ai_handler.h"
#include <opencv2/opencv.hpp>
#include <thread>
#include "mqtt_publisher.h"
#include "json.hpp"
#include "world_state.h"
#include <iostream>

using json = nlohmann::json;
using namespace cv;
using namespace std;

// This is the main processing thread for the application.
void detectionLoop(const Mat& cameraMatrix, const Mat& distCoeffs,
                   float markerLength, SharedState& state) {

    // --- INITIALIZATION ---
    MQTTPublisher mqtt("tcp://192.168.0.122:1883", "robots/commands");
    mqtt.connect();

    AIHandler ai_handler("RobotSoccerTeamA.onnx");

    auto lastUpdate = chrono::steady_clock::now();

    // --- MAIN LOOP ---
    while (state.running) {
        // 1. GET LATEST FRAME (runs on every loop)
        Mat frame;
        {
            lock_guard<mutex> lock(state.frameMutex);
            if (state.sharedFrame.empty()) {
                this_thread::sleep_for(chrono::milliseconds(10));
                continue;
            }
            state.sharedFrame.copyTo(frame);
        }

        Mat displayFrame = frame.clone();

        // 2. DETECT EVERYTHING (runs on every loop)
        vector<Ball> currentBalls = detectOrangeBalls(frame);
        Mat current_H = detectArenaMarkers(frame, displayFrame, cameraMatrix, distCoeffs, markerLength, currentBalls);
        vector<DetectedBot> current_bots = detectBots(frame, displayFrame, cameraMatrix, distCoeffs, markerLength);

        // 3. UPDATE SHARED STATE (runs on every loop)
        {
            lock_guard<mutex> lock(state.dataMutex);
            if (!current_H.empty()) {
                state.last_known_H = current_H;
            }
            if (!current_bots.empty()) {
                state.last_known_bots = current_bots;
            }
        }

        // --- AI, PUBLISHING, AND VISUALIZATION (Throttled to run ~10 times per second) ---
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastUpdate).count();

        if (elapsed >= 100) {
            lastUpdate = now; // Reset the timer

            // 4. BUILD WORLD STATE for the AI
            WorldState world;
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

            // 5. GET MOVEMENT COMMANDS from the AI
            auto commands = ai_handler.predictMovements(world);

            // 6. BUILD AND PUBLISH COMMANDS via MQTT
            if (!commands.empty()) {
                json command_list = json::array();
                for (const auto& [id, cmd] : commands) {
                    command_list.push_back({{"id", id}, {"left", cmd.left}, {"right", cmd.right}});
                }
                json command_payload = {{"commands", command_list}};
                cout << "Publishing AI Commands: " << command_payload.dump() << endl;
                mqtt.publish(command_payload.dump());
            }

            // 7. DRAW TOP-DOWN VIEW
            Mat topDownMap = Mat::zeros(480, 480, CV_8UC3);
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

        // --- Show the main camera view (runs on every loop) ---
        imshow("Arena View", displayFrame);
        if (waitKey(1) == 'q') {
            state.running = false;
            break;
        }
    }
}

// This is the camera thread loop. It's separate from the detection handler.
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