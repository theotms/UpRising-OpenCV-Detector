// camera_handler.cpp
#include <opencv2/opencv.hpp>       // OpenCV core functionality
#include <thread>                   // For multithreading
#include <atomic>                   // For thread-safe boolean
#include <mutex>                    // For thread-safe access to shared data

#include "camera_handler.h"

std::atomic<bool> running(true);
cv::Mat sharedFrame;
std::mutex frameMutex;

void captureLoop(cv::VideoCapture& cap) {
    cv::Mat local;
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
