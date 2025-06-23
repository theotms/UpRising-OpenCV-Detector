#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>
#include <iostream>

#include "ball_detector.h"
#include "arena_detector.h"
#include "bot_detector.h"
#include <curl/curl.h>

using namespace cv;
using namespace std;
using json = nlohmann::json;

// Shared resources
std::atomic<bool> running(true);
cv::Mat sharedFrame;
std::mutex frameMutex;
std::mutex ballMutex;
std::vector<Ball> detectedBalls;

void captureLoop(VideoCapture& cap) {
    Mat local;
    while (running) {
        cap >> local;
        if (local.empty()) continue;

        {
            std::lock_guard<std::mutex> lock(frameMutex);
            local.copyTo(sharedFrame);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void triggerRobotMove() {
    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.0.117/move");  // Change IP if needed
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L); // 2 seconds timeout
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
        }
        curl_easy_cleanup(curl);
    }
}

void detectionLoop(Mat cameraMatrix, Mat distCoeffs, float markerLength) {
    bool previouslyDetected = false;
    auto lastTrigger = chrono::steady_clock::now();

    while (running) {
        Mat frame;
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            if (!sharedFrame.empty())
                sharedFrame.copyTo(frame);
        }

        if (frame.empty()) continue;

        // 1. Ball detection
        std::vector<Ball> currentBalls = detectOrangeBalls(frame);

        {
            std::lock_guard<std::mutex> lock(ballMutex);
            detectedBalls = currentBalls;
        }

        // ✅ TRIGGER LOGIC
        if (!currentBalls.empty()) {
            auto now = chrono::steady_clock::now();
            auto elapsed = chrono::duration_cast<chrono::seconds>(now - lastTrigger).count();

            if (!previouslyDetected || elapsed >= 5) {  // cooldown: 5 seconds
                triggerRobotMove();
                lastTrigger = now;
                previouslyDetected = true;
            }
        } else {
            previouslyDetected = false;
        }

        // 2. Arena detection & top-down view with balls
        detectArenaMarkers(frame, cameraMatrix, distCoeffs, markerLength, currentBalls);

        // 3. Bot detection (IDs 1–4)
        detectBots(frame, cameraMatrix, distCoeffs, markerLength);

        // 4. HUD
        putText(frame, "Balls: " + to_string(currentBalls.size()),
                Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.6,
                currentBalls.empty() ? Scalar(0, 0, 255) : Scalar(0, 255, 0), 2);

        imshow("Arena View", frame);

        if (waitKey(1) == 'q') {
            running = false;
            break;
        }
    }
}

int main() {
    // Load camera calibration from JSON
    ifstream file("rpi-camera-calib-params.json");
    if (!file) {
        cerr << "Calibration file not found!" << endl;
        return -1;
    }

    json calib;
    file >> calib;
    Mat cameraMatrix(3, 3, CV_32F);
    Mat distCoeffs(1, 5, CV_32F);

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            cameraMatrix.at<float>(i, j) = calib["mtx"][i][j];
    for (int i = 0; i < 5; i++)
        distCoeffs.at<float>(0, i) = calib["dist"][i];

    float markerLength = 0.15f; // meters

    // Open camera
    VideoCapture cap("/dev/video2", CAP_V4L2);
    if (!cap.isOpened()) {
        cerr << "Failed to open camera." << endl;
        return -1;
    }
    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);

    // Threads
    thread camThread(captureLoop, std::ref(cap));
    thread detectThread(detectionLoop, cameraMatrix, distCoeffs, markerLength);

    camThread.join();
    detectThread.join();

    return 0;
}
