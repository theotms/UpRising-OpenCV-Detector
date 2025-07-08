#include "arena_detector.h"
#include <opencv2/aruco.hpp>
#include <iostream>

using namespace cv;
using namespace std;

Mat detectArenaMarkers(const Mat& frame, Mat& displayFrame, const Mat& cameraMatrix, const Mat& distCoeffs, float markerLength, const vector<Ball>& balls) {
    vector<int> ids;
    vector<vector<Point2f>> corners;
    auto dictionary = aruco::getPredefinedDictionary(aruco::DICT_4X4_50);
    aruco::detectMarkers(frame, dictionary, corners, ids);

    map<int, Point2f> marker_centers;
    if (!ids.empty()) {
        for (int i = 0; i < ids.size(); ++i) {
            if (ids[i] >= 46 && ids[i] <= 49) { // Arena markers
                Point2f center = (corners[i][0] + corners[i][1] + corners[i][2] + corners[i][3]) / 4;
                marker_centers[ids[i]] = center;
            }
        }
    }

    // Draw detected balls onto the display frame
    for (const auto& ball : balls) {
        circle(displayFrame, ball.center, ball.radius, Scalar(0, 255, 255), 2);
    }

    if (marker_centers.count(46) && marker_centers.count(47) && marker_centers.count(48) && marker_centers.count(49)) {
        vector<Point2f> src_pts = {marker_centers[46], marker_centers[47], marker_centers[48], marker_centers[49]};
        vector<Point2f> dst_pts = {Point2f(0, 480), Point2f(0, 0), Point2f(480, 0), Point2f(480, 480)};

        Mat h = findHomography(src_pts, dst_pts);

        // --- THE FIX IS HERE ---
        // Original points are Point2f (float)
        vector<Point2f> frame_corners_f = {marker_centers[47], marker_centers[48], marker_centers[49], marker_centers[46]};

        // Create a new vector of Point (integer) by converting the float points.
        // This is what the polylines function requires.
        vector<Point> frame_corners_i(frame_corners_f.begin(), frame_corners_f.end());

        // Draw the arena outline using the integer points.
        polylines(displayFrame, frame_corners_i, true, Scalar(255, 0, 255), 2);

        return h;
    }

    return Mat();
}