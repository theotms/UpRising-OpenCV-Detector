#ifndef BALL_DETECTOR_H
#define BALL_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <vector>


struct Ball {
    cv::Point2f center;
    float radius;
    int id;
};

std::vector<Ball> detectOrangeBalls(const cv::Mat& frame);

#endif // BALL_DETECTOR_H
