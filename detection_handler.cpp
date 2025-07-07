#include "detection_handler.h"
#include "arena_detector.h"
#include "bot_detector.h"
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
            if (!state.sharedFrame.empty())
                state.sharedFrame.copyTo(frame);
        }

        if (frame.empty()) continue;

        // --- DETECTION LOGIC (WITH MEMORY) ---

        // 1. Detect balls FIRST, as detectArenaMarkers needs this info.
        vector<Ball> currentBalls = detectOrangeBalls(frame);

        // 2. Detect arena markers. If successful, update our memory.
        //    CORRECTED: Added 'currentBalls' back to the function call.
        Mat current_H = detectArenaMarkers(frame, cameraMatrix, distCoeffs, markerLength, currentBalls);
        if (!current_H.empty()) {
            lock_guard<mutex> lock(state.dataMutex);
            state.last_known_H = current_H;
        }

        // 3. Detect bots. If successful, update our memory.
        vector<DetectedBot> current_bots = detectBots(frame, cameraMatrix, distCoeffs, markerLength);
        if (!current_bots.empty()) {
            lock_guard<mutex> lock(state.dataMutex);
            state.last_known_bots = current_bots;
        }


        // --- PUBLISHING LOGIC (ALWAYS USES MEMORY) ---
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastWorldPublished).count();

        if (elapsed >= 1000) {
            WorldState world;
            Mat H_for_transform;
            vector<DetectedBot> bots_to_transform;

            // Get a consistent snapshot of the data from memory
            {
                lock_guard<mutex> lock(state.dataMutex);
                H_for_transform = state.last_known_H;
                bots_to_transform = state.last_known_bots;
            }

            // Only proceed if we have a valid transform AND bots to transform
            if (!H_for_transform.empty() && !bots_to_transform.empty()) {
                // Transform bot coordinates to the top-down map view
                vector<Point2f> bot_centers_in, bot_centers_out;
                for(const auto& bot : bots_to_transform) {
                    bot_centers_in.push_back(bot.center);
                }
                perspectiveTransform(bot_centers_in, bot_centers_out, H_for_transform);

                // Build world state using the transformed coordinates
                for (size_t i = 0; i < bots_to_transform.size(); ++i) {
                    Bot b;
                    b.id = bots_to_transform[i].id;
                    b.center = bot_centers_out[i];
                    b.angle = bots_to_transform[i].angleDeg;
                    b.is_ai = bots_to_transform[i].isAI;
                    world.bots.push_back(b);
                }
            }

            // Transform ball coordinates if a ball is visible
            if (!currentBalls.empty() && !H_for_transform.empty()) {
                vector<Point2f> ball_in = {currentBalls.front().center}, ball_out;
                perspectiveTransform(ball_in, ball_out, H_for_transform);
                world.ball.center = ball_out[0];
            }

            // Publish the world state, which will be empty if memory is empty, but stable otherwise
            json j = world;
            std::cout << "Publishing: " << j.dump() << std::endl;
            mqtt.publish(j.dump());
            lastWorldPublished = now;
        }

        // --- HUD and frame output (no change) ---
        imshow("Arena View", frame);

        if (waitKey(1) == 'q') {
            state.running = false;
            break;
        }
    }
}

// The captureLoop function remains completely unchanged.
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