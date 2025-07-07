// PASTE THIS ENTIRE FUNCTION INTO ArenaDetection.cpp TO REPLACE THE OLD ONE

#include "arena_detector.h"
#include <opencv2/aruco.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <map>

using namespace cv;
using namespace std;
using namespace aruco;

// --- Persistent state for smoothing ---
static map<int, Vec3d> smoothedRvecs;
static map<int, Vec3d> smoothedTvecs;
static map<int, int> markerSeenCount;
const double alpha = 0.2;

cv::Mat detectArenaMarkers(Mat& frame, const Mat& cameraMatrix, const Mat& distCoeffs,
                           float markerLength, const std::vector<Ball>& balls) {

    // NEW: Make the last good transformation matrix and top-down view persistent
    static Mat lastGoodH;
    static Mat lastGoodTopDownView;

    // NEW: Initialize the top-down view to a black screen on the very first run
    if (lastGoodTopDownView.empty()) {
        lastGoodTopDownView = Mat::zeros(400, 400, CV_8UC3);
    }

    // --- Detection logic (mostly unchanged) ---
    Ptr<aruco::DetectorParameters> params = aruco::DetectorParameters::create();
    params->cornerRefinementMethod = aruco::CORNER_REFINE_SUBPIX;
    // ... (other params are the same)

    Ptr<Dictionary> dictionary = getPredefinedDictionary(DICT_4X4_50);
    vector<int> ids;
    vector<vector<Point2f>> corners;
    detectMarkers(frame, dictionary, corners, ids, params);

    // --- Pose estimation (unchanged) ---
    vector<Point3f> objectPoints = {
        {-markerLength/2,  markerLength/2, 0}, { markerLength/2,  markerLength/2, 0},
        { markerLength/2, -markerLength/2, 0}, {-markerLength/2, -markerLength/2, 0}
    };
    map<int, vector<Point2f>> topDownCorners;
    for (size_t i = 0; i < ids.size(); ++i) {
        // ... (the entire for loop for pose estimation and smoothing is unchanged) ...
        int id = ids[i];
        markerSeenCount[id]++;
        if (markerSeenCount[id] < 3) continue;
        if (corners[i].size() != 4) continue;
        Vec3d rvec, tvec;
        solvePnP(objectPoints, corners[i], cameraMatrix, distCoeffs, rvec, tvec);
        if (smoothedRvecs.count(id)) rvec = alpha * rvec + (1.0 - alpha) * smoothedRvecs[id];
        smoothedRvecs[id] = rvec;
        if (smoothedTvecs.count(id)) tvec = alpha * tvec + (1.0 - alpha) * smoothedTvecs[id];
        smoothedTvecs[id] = tvec;
        drawFrameAxes(frame, cameraMatrix, distCoeffs, rvec, tvec, 0.05f);
        Point2f center(0, 0);
        for (const auto& pt : corners[i]) center += pt;
        center *= 0.25f;
        putText(frame, "Marker " + to_string(id), center, FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0), 2);
        if (id >= 46 && id <= 49) {
            topDownCorners[id] = corners[i];
        }
    }

    // --- MODIFIED: Top-down view generation ---

    // 1. If we see all 4 corners, we calculate a NEW, updated transformation matrix
    if (topDownCorners.size() == 4) {
        auto markerCenter = [](const vector<Point2f>& pts) {
            Point2f c(0, 0); for (const auto& p : pts) c += p; return c * 0.25f;
        };
        vector<Point2f> srcPoints = {
            markerCenter(topDownCorners[46]), markerCenter(topDownCorners[47]),
            markerCenter(topDownCorners[48]), markerCenter(topDownCorners[49])
        };
        vector<Point2f> dstPoints = {
            Point2f(50, 50), Point2f(350, 50), Point2f(350, 350), Point2f(50, 350)
        };

        // Update the persistent matrix with the new, good one
        lastGoodH = getPerspectiveTransform(srcPoints, dstPoints);
    }

    // 2. If we have a valid transformation matrix (either new or old), we generate the view
    if (!lastGoodH.empty()) {
        // Warp the CURRENT camera frame using the last known good perspective
        warpPerspective(frame, lastGoodTopDownView, lastGoodH, lastGoodTopDownView.size());

        // Draw the arena boundary on the main frame using the same transform
        vector<Point2f> dstPoints = { Point2f(50, 50), Point2f(350, 50), Point2f(350, 350), Point2f(50, 350) };
        vector<Point2f> warpedBack;
        perspectiveTransform(dstPoints, warpedBack, lastGoodH.inv());
        for (int i = 0; i < 4; ++i)
            line(frame, warpedBack[i], warpedBack[(i + 1) % 4], Scalar(0, 255, 0), 2);

        // Project the CURRENT ball positions into the top-down view
        for (const auto& ball : balls) {
            if (ball.center.x > 0 && ball.center.y > 0) {
                vector<Point2f> in = {ball.center}, out;
                perspectiveTransform(in, out, lastGoodH);
                circle(lastGoodTopDownView, out[0], 8, Scalar(0, 0, 255), FILLED);
                putText(lastGoodTopDownView, "BALL", out[0] + Point2f(10, 0), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255), 1);
            }
        }
    }

    // 3. Show the persistent top-down view. It will either be the newly rendered frame
    // or the last good frame if the markers were lost.
    imshow("Top-down View", lastGoodTopDownView);

    // Return the last good matrix for the detection_handler to use
    return lastGoodH;
}