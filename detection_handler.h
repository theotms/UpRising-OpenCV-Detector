#ifndef DETECTION_HANDLER_H
#define DETECTION_HANDLER_H

#include <opencv2/opencv.hpp>
#include <atomic>
#include <mutex>
#include <vector>
#include "ball_detector.h"
#include "bot_detector.h" // <-- ADDED: We need to know about DetectedBot
#include "json.hpp"

using json = nlohmann::json;

struct SharedState {
    std::atomic<bool> running;
    cv::Mat sharedFrame;
    std::mutex frameMutex;

    // --- MODIFIED STATE ---
    std::mutex dataMutex; // A single mutex for all our shared data
    std::vector<DetectedBot> last_known_bots; // Memory for bots
    cv::Mat last_known_H; // Memory for the perspective transform

    SharedState() : running(true) {}
};

void captureLoop(cv::VideoCapture& cap, SharedState& state);
void detectionLoop(const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs,
                   float markerLength, SharedState& state);

#endif // DETECTION_HANDLER_H