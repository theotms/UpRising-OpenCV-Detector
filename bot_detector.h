#ifndef BOT_DETECTOR_H
#define BOT_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

// Detects bots (ArUco markers 1–4) and draws labels + pose axes.
// 1 & 2 = player-controlled bots; 3 & 4 = AI bots.
void detectBots(cv::Mat& frame,
                const cv::Mat& cameraMatrix,
                const cv::Mat& distCoeffs,
                float markerLength);

#endif // BOT_DETECTOR_H
