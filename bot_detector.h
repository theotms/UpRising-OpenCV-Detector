#ifndef CAM_ARUCO_BOT_DETECTOR_H
#define CAM_ARUCO_BOT_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <vector>

struct DetectedBot {
    int id;
    cv::Point2f center;
    float angleDeg;
    bool isAI;
};

// UPDATED FUNCTION SIGNATURE
std::vector<DetectedBot> detectBots(const cv::Mat& frame, cv::Mat& displayFrame, const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs, float markerLength);

#endif //CAM_ARUCO_BOT_DETECTOR_H