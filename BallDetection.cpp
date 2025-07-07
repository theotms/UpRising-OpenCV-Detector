//
// Created by nischal on 6/10/25.
//
#include "ball_detector.h"
#include <opencv2/opencv.hpp>
#include "json.hpp"
using json = nlohmann::json;
using namespace cv;
using namespace std;

vector<Ball> detectOrangeBalls(const Mat& frame) {
    Mat hsv, mask;
    cvtColor(frame, hsv, COLOR_BGR2HSV);

    Scalar lowerOrange(5, 100, 100);
    Scalar upperOrange(25, 255, 255);
    inRange(hsv, lowerOrange, upperOrange, mask);

    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
    morphologyEx(mask, mask, MORPH_OPEN, kernel);
    morphologyEx(mask, mask, MORPH_CLOSE, kernel);

    vector<vector<Point>> contours;
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    vector<Ball> balls;
    int ballId = 1;
    for (const auto& contour : contours) {
        float area = contourArea(contour);
        if (area > 100) {
            Point2f center;
            float radius;
            minEnclosingCircle(contour, center, radius);
            float circularity = area / (CV_PI * radius * radius);
            if (circularity > 0.6) {
                balls.push_back({center});
            }
        }
    }
    return balls;
}
