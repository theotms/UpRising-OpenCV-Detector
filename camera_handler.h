// camera_handler.h
#ifndef CAMERA_HANDLER_H
#define CAMERA_HANDLER_H

#include <opencv2/opencv.hpp>
#include <atomic>
#include <mutex>
#include "json.hpp"
using json = nlohmann::json;


extern std::atomic<bool> running;
extern cv::Mat sharedFrame;
extern std::mutex frameMutex;

void captureLoop(cv::VideoCapture& cap);

#endif
