#ifndef BOT_DETECTOR_H
#define BOT_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include "json.hpp"
using json = nlohmann::json;

// Bot structure for world state
struct DetectedBot {
    int id;
    cv::Point2f center;
    float angleDeg;
    bool isAI;
};

std::vector<DetectedBot> detectBots(cv::Mat& frame, const cv::Mat& cameraMatrix,
                                    const cv::Mat& distCoeffs, float markerLength);

#endif // BOT_DETECTOR_H
