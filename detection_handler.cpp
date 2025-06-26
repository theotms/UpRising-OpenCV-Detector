#include "detection_handler.h"
#include "arena_detector.h"
#include "bot_detector.h"
#include <opencv2/opencv.hpp>
#include <thread>
#include "mqtt_publisher.h"


using namespace cv;
using namespace std;

void detectionLoop(const Mat& cameraMatrix, const Mat& distCoeffs,
                   float markerLength, SharedState& state) {

    MQTTPublisher mqtt("tcp://192.168.0.122:1883", "arena/ball");
    mqtt.connect();

    auto lastPublished = chrono::steady_clock::now();

    while (state.running) {
        Mat frame;
        {
            lock_guard<mutex> lock(state.frameMutex);
            if (!state.sharedFrame.empty())
                state.sharedFrame.copyTo(frame);
        }

        if (frame.empty()) continue;

        // Detect balls first
        vector<Ball> currentBalls = detectOrangeBalls(frame);
        {
            lock_guard<mutex> lock(state.ballMutex);
            state.detectedBalls = currentBalls;
        }

        // Debounced publishing: only send if interval passed
        if (!currentBalls.empty()) {
            auto now = chrono::steady_clock::now();
            auto elapsed = chrono::duration_cast<chrono::seconds>(now - lastPublished).count();

            if (elapsed >= 3) {  // wait at least 3 seconds between sends
                mqtt.publish("BALL_DETECTED");
                lastPublished = now;
            }
        }

        // Continue with arena and bot detection
        detectArenaMarkers(frame, cameraMatrix, distCoeffs, markerLength, currentBalls);
        detectBots(frame, cameraMatrix, distCoeffs, markerLength);

        // HUD
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

