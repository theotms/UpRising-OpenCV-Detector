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

    auto lastBallPublished = chrono::steady_clock::now();
    auto lastWorldPublished = chrono::steady_clock::now();

    while (state.running) {
        Mat frame;
        {
            lock_guard<mutex> lock(state.frameMutex);
            if (!state.sharedFrame.empty())
                state.sharedFrame.copyTo(frame);
        }

        if (frame.empty()) continue;

        // === Detect balls (no change) ===
        vector<Ball> currentBalls = detectOrangeBalls(frame);
        {
            lock_guard<mutex> lock(state.ballMutex);
            state.detectedBalls = currentBalls;
        }

        // === BALL_DETECTED signal (no change) ===
        if (!currentBalls.empty()) {
            // ... (this block is unchanged) ...
        }

        // === MODIFIED LOGIC FLOW ===

        // 1. Detect arena and get the perspective transform matrix
        Mat H = detectArenaMarkers(frame, cameraMatrix, distCoeffs, markerLength, currentBalls);

        // 2. Only proceed if the arena was successfully found (H is not empty)
        if (!H.empty()) {
            // 3. Detect bots to get their raw pixel coordinates
            vector<DetectedBot> bots = detectBots(frame, cameraMatrix, distCoeffs, markerLength);

            // 4. NEW: Transform bot coordinates to the top-down map view
            vector<Point2f> bot_centers_in, bot_centers_out;
            for(const auto& bot : bots) {
                bot_centers_in.push_back(bot.center);
            }

            if (!bot_centers_in.empty()) {
                perspectiveTransform(bot_centers_in, bot_centers_out, H);
            }

            // 5. Build world state using the NEW transformed coordinates
            WorldState world;
            for (size_t i = 0; i < bots.size(); ++i) {
                Bot b;
                b.id = bots[i].id;
                b.center = bot_centers_out[i]; // Use the transformed center
                b.angle = bots[i].angleDeg;
                b.is_ai = bots[i].isAI;
                world.bots.push_back(b);
            }

            if (!currentBalls.empty()) {
                // Also transform the ball's coordinates
                vector<Point2f> ball_in = {currentBalls.front().center}, ball_out;
                perspectiveTransform(ball_in, ball_out, H);
                world.ball.center = ball_out[0];
            }

            // 6. Publish full world state (1Hz)
            auto now = chrono::steady_clock::now();
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastWorldPublished).count();

            if (elapsed >= 1000) {
                json j = world;
                std::cout << "Publishing: " << j.dump() << std::endl;
                mqtt.publish(j.dump());
                lastWorldPublished = now;
            }
        }

        // === HUD and frame output (no change) ===
        putText(frame, "Balls: " + to_string(currentBalls.size()), Point(10, 30),
            FONT_HERSHEY_SIMPLEX, 0.6,
            currentBalls.empty() ? Scalar(0, 0, 255) : Scalar(0, 255, 0), 2);

        imshow("Arena View", frame);

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

