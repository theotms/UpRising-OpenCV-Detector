#ifndef ARENA_DETECTOR_H
#define ARENA_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <vector>
#include "ball_detector.h"
#include "json.hpp"
using json = nlohmann::json;


// Detects arena corner markers and draws arena + top-down view.
// Takes previously detected balls to overlay them on the top-down view.
void detectArenaMarkers(cv::Mat& frame,
                        const cv::Mat& cameraMatrix,
                        const cv::Mat& distCoeffs,
                        float markerLength,
                        const std::vector<Ball>& balls);

#endif // ARENA_DETECTOR_H
