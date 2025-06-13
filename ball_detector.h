//
// Created by nischal on 6/10/25.
//

#ifndef BALL_DETECTOR_H
#define BALL_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <vector>

struct Ball {
    cv::Point2f position;
    float radius;
    int id;
};

// Detects orange balls in a frame and returns their info.
std::vector<Ball> detectOrangeBalls(const cv::Mat& frame);

// Returns a unique color for each ball ID (cycling through a palette).
cv::Scalar getBallColor(int id);

#endif // BALL_DETECTOR_H
