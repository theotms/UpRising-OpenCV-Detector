#include "detection_handler.h"
#include "arena_detector.h"
#include "bot_detector.h"
#include "ball_detector.h"
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

    MQTTPublisher mqtt("tcp://192.168.0.122:1883", "arena/world");
    mqtt.connect();

    auto lastWorldPublished = chrono::steady_clock::now();

    while (state.running) {
        Mat frame;
        {
            lock_guard<mutex> lock(state.frameMutex);
            if (state.sharedFrame.empty()) continue;
            state.sharedFrame.copyTo(frame);
        }

        Mat displayFrame = frame.clone();

        // --- DETECTION LOGIC ---
        vector<Ball> currentBalls = detectOrangeBalls(frame);
        Mat current_H = detectArenaMarkers(frame, displayFrame, cameraMatrix, distCoeffs, markerLength, currentBalls);
        if (!current_H.empty()) {
            lock_guard<mutex> lock(state.dataMutex);
            state.last_known_H = current_H;
        }

        vector<DetectedBot> current_bots = detectBots(frame, displayFrame, cameraMatrix, distCoeffs, markerLength);
        if (!current_bots.empty()) {
            lock_guard<mutex> lock(state.dataMutex);
            state.last_known_bots = current_bots;
        }

        // --- PUBLISHING & TOP-DOWN VIEW LOGIC ---
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastWorldPublished).count();

        if (elapsed >= 100) {
            WorldState world;
            Mat H_for_transform;
            vector<DetectedBot> bots_to_transform;

            {
                lock_guard<mutex> lock(state.dataMutex);
                H_for_transform = state.last_known_H;
                bots_to_transform = state.last_known_bots;
            }

            if (!H_for_transform.empty()) {
                // Transform bots if they exist
                if (!bots_to_transform.empty()) {
                    vector<Point2f> bot_centers_in, bot_centers_out;
                    for(const auto& bot : bots_to_transform) {
                        bot_centers_in.push_back(bot.center);
                    }
                    perspectiveTransform(bot_centers_in, bot_centers_out, H_for_transform);

                    for (size_t i = 0; i < bots_to_transform.size(); ++i) {
                        world.bots.push_back({
                            bots_to_transform[i].id,
                            bot_centers_out[i],
                            bots_to_transform[i].angleDeg,
                            bots_to_transform[i].isAI
                        });
                    }
                }

                // Transform ball if it exists
                if (!currentBalls.empty()) {
                    vector<Point2f> ball_in = {currentBalls.front().center}, ball_out;
                    perspectiveTransform(ball_in, ball_out, H_for_transform);
                    world.ball.center = ball_out[0];
                    world.ball.radius = currentBalls.front().radius; // Still useful data, even if not used for drawing
                }
            }

            // Publish the world state
            json j = world;
            std::cout << "Publishing: " << j.dump() << std::endl;
            mqtt.publish(j.dump());
            lastWorldPublished = now;

            // --- RE-IMPLEMENTED TOP DOWN VIEW ---
            Mat topDownMap = Mat::zeros(480, 480, CV_8UC3);

            // Draw the ball on the map
            if (world.ball.radius > 0) {
                circle(topDownMap, world.ball.center, 10, Scalar(0, 165, 255), -1); // Orange ball
            }

            // Draw the bots on the map
            for (const auto& bot : world.bots) {
                // Represent bot as a rotated rectangle
                RotatedRect botRect(bot.center, Size2f(30, 25), bot.angle);
                Point2f vertices[4];
                botRect.points(vertices);
                for (int i = 0; i < 4; i++) {
                    line(topDownMap, vertices[i], vertices[(i + 1) % 4], Scalar(255, 0, 0), 2); // Blue bot
                }
                // Draw a line indicating the front
                Point2f front_point(
                    bot.center.x + 15 * cos(bot.angle * CV_PI / 180.0),
                    bot.center.y + 15 * sin(bot.angle * CV_PI / 180.0)
                );
                line(topDownMap, bot.center, front_point, Scalar(0, 255, 0), 2); // Green line for front
            }
            imshow("Top Down View", topDownMap);
        }

        // Show the main camera feed
        imshow("Arena View", displayFrame);

        if (waitKey(1) == 'q') {
            state.running = false;
            break;
        }
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