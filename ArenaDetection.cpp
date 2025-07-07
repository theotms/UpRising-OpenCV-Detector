#include "arena_detector.h"
#include <opencv2/aruco.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>

using namespace cv;
using namespace std;
using namespace aruco;

// === Persistent state for smoothing ===
static map<int, Vec3d> smoothedRvecs;
static map<int, Vec3d> smoothedTvecs;
static map<int, int> markerSeenCount;
const double alpha = 0.2;  // Smoothing factor

void detectArenaMarkers(Mat& frame, const Mat& cameraMatrix, const Mat& distCoeffs,
                        float markerLength, const std::vector<Ball>& balls) {
    // 1. Set custom detection parameters (improves stability)
    Ptr<aruco::DetectorParameters> params = aruco::DetectorParameters::create();
    params->cornerRefinementMethod = aruco::CORNER_REFINE_SUBPIX;
    params->cornerRefinementWinSize = 5;
    params->minMarkerDistanceRate = 0.05;
    params->cornerRefinementMinAccuracy = 0.5;

    // 2. Detect markers
    Ptr<Dictionary> dictionary = getPredefinedDictionary(DICT_4X4_50);
    vector<int> ids;
    vector<vector<Point2f>> corners;
    detectMarkers(frame, dictionary, corners, ids, params);
    if (ids.empty()) return;

    // 3. Common 3D points for all markers
    vector<Point3f> objectPoints = {
        {-markerLength/2,  markerLength/2, 0},
        { markerLength/2,  markerLength/2, 0},
        { markerLength/2, -markerLength/2, 0},
        {-markerLength/2, -markerLength/2, 0}
    };

    map<int, vector<Point2f>> topDownCorners;

    for (size_t i = 0; i < ids.size(); ++i) {
        int id = ids[i];
        markerSeenCount[id]++;

        if (markerSeenCount[id] < 3) continue;  // Skip unstable markers
        if (corners[i].size() != 4) continue;

        // Estimate pose
        Vec3d rvec, tvec;
        solvePnP(objectPoints, corners[i], cameraMatrix, distCoeffs, rvec, tvec);

        // Smooth pose (Exponential Moving Average)
        if (smoothedRvecs.count(id))
            rvec = alpha * rvec + (1.0 - alpha) * smoothedRvecs[id];
        smoothedRvecs[id] = rvec;

        if (smoothedTvecs.count(id))
            tvec = alpha * tvec + (1.0 - alpha) * smoothedTvecs[id];
        smoothedTvecs[id] = tvec;

        // Draw axis on frame
        drawFrameAxes(frame, cameraMatrix, distCoeffs, rvec, tvec, 0.05f);

        // Label marker
        Point2f center(0, 0);
        for (const auto& pt : corners[i]) center += pt;
        center *= 0.25f;
        putText(frame, "Marker " + to_string(id), center,
                FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0), 2);

        // Track arena boundary markers
        if (id >= 46 && id <= 49) {
            topDownCorners[id] = corners[i];
        }
    }

    // 4. Generate top-down view
    Mat topDown = Mat::zeros(400, 400, CV_8UC3);

    if (topDownCorners.size() == 4) {
        auto markerCenter = [](const vector<Point2f>& pts) {
            Point2f c(0, 0);
            for (const auto& p : pts) c += p;
            return c * 0.25f;
        };

        vector<Point2f> srcPoints = {
            markerCenter(topDownCorners[46]),  // Top-left
            markerCenter(topDownCorners[47]),  // Top-right
            markerCenter(topDownCorners[48]),  // Bottom-right
            markerCenter(topDownCorners[49])   // Bottom-left
        };

        vector<Point2f> dstPoints = {
            Point2f(50, 50), Point2f(350, 50),
            Point2f(350, 350), Point2f(50, 350)
        };

        Mat H = getPerspectiveTransform(srcPoints, dstPoints);

        // Warp original frame into top-down
        warpPerspective(frame, topDown, H, Size(400, 400));

        // Draw arena boundary on original frame
        vector<Point2f> warpedBack;
        perspectiveTransform(dstPoints, warpedBack, H.inv());

        for (int i = 0; i < 4; ++i)
            line(frame, warpedBack[i], warpedBack[(i + 1) % 4], Scalar(0, 255, 0), 2);

        // Project balls into top-down view
        for (const auto& ball : balls) {
            if (ball.center.x > 0 && ball.center.y > 0) {
                vector<Point2f> in = {ball.center}, out;
                perspectiveTransform(in, out, H);

                circle(topDown, out[0], 8, Scalar(0, 0, 255), FILLED);
                putText(topDown, "BALL", out[0] + Point2f(10, 0),
                        FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255), 1);
            }
        }
    }

    // Show outputs
    imshow("Top-down View", topDown);
}
