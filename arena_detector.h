#ifndef CAM_ARUCO_ARENA_DETECTOR_H
#define CAM_ARUCO_ARENA_DETECTOR_H

#include <opencv2/opencv.hpp>
#include "ball_detector.h" // Include for Ball struct

// UPDATED FUNCTION SIGNATURE
cv::Mat detectArenaMarkers(const cv::Mat& frame, cv::Mat& displayFrame, const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs, float markerLength, const std::vector<Ball>& balls);

#endif //CAM_ARUCO_ARENA_DETECTOR_H