#include "ball_detector.h"
#include <opencv2/opencv.hpp>
#include "json.hpp"
using json = nlohmann::json;
using namespace cv;
using namespace std;

vector<Ball> detectOrangeBalls(const Mat& frame) {
    Mat hsv, mask;
    cvtColor(frame, hsv, COLOR_BGR2HSV);

    // These HSV values seem to be working for you, so we'll keep them.
    Scalar lowerOrange(0, 119, 210);
    Scalar upperOrange(51, 196, 255);
    inRange(hsv, lowerOrange, upperOrange, mask);

    // Clean up the mask to remove noise
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
    morphologyEx(mask, mask, MORPH_OPEN, kernel);
    morphologyEx(mask, mask, MORPH_CLOSE, kernel);

    vector<vector<Point>> contours;
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    vector<Ball> balls;
    for (const auto& contour : contours) {
        float area = contourArea(contour);
        // Filter out small, noisy contours
        if (area > 100) {
            Point2f center;
            float radius;
            minEnclosingCircle(contour, center, radius);

            // Check if the contour is reasonably circular
            float circularity = area / (CV_PI * radius * radius);
            if (circularity > 0.6) {
                balls.push_back({center, radius});
            }
        }
    }
    return balls;
}