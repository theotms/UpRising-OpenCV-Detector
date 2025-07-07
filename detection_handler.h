#ifndef DETECTION_HANDLER_H
#define DETECTION_HANDLER_H

#include <opencv2/opencv.hpp>
#include <atomic>
#include <mutex>
#include <vector>
#include "ball_detector.h"
#include "json.hpp"
using json = nlohmann::json;


struct SharedState {
    std::atomic<bool> running;
    cv::Mat sharedFrame;
    std::mutex frameMutex;
    std::mutex ballMutex;
    std::vector<Ball> detectedBalls;

    SharedState() : running(true) {}
};

void captureLoop(cv::VideoCapture& cap, SharedState& state);
void detectionLoop(const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs,
                   float markerLength, SharedState& state);

#endif // DETECTION_HANDLER_H
