#include "arena_detector.h"

using namespace cv;
using namespace std;
using namespace aruco;

void detectArenaMarkers(Mat& frame, const Mat& cameraMatrix, const Mat& distCoeffs,
                        float markerLength, const vector<Ball>& balls) {
    Ptr<Dictionary> dictionary = getPredefinedDictionary(DICT_4X4_50);
    Ptr<DetectorParameters> parameters = makePtr<DetectorParameters>();
    vector<int> ids;
    vector<vector<Point2f>> corners;

    detectMarkers(frame, dictionary, corners, ids, parameters);
    if (ids.empty()) return;

    vector<Vec3d> rvecs, tvecs;
    estimatePoseSingleMarkers(corners, markerLength, cameraMatrix, distCoeffs, rvecs, tvecs);
    drawDetectedMarkers(frame, corners, ids);

    map<int, Point2f> markerCenters;
    for (size_t i = 0; i < ids.size(); ++i) {
        Point2f center(0, 0);
        for (const auto& pt : corners[i]) center += pt;
        center *= 0.25f;
        markerCenters[ids[i]] = center;

        if (ids[i] == 46 || ids[i] == 47 || ids[i] == 48 || ids[i] == 49) {
            drawFrameAxes(frame, cameraMatrix, distCoeffs, rvecs[i], tvecs[i], markerLength * 0.5f);
        }
    }

    if (markerCenters.count(46) && markerCenters.count(47) &&
        markerCenters.count(48) && markerCenters.count(49)) {

        line(frame, markerCenters[46], markerCenters[47], Scalar(0, 255, 0), 2);
        line(frame, markerCenters[47], markerCenters[48], Scalar(0, 255, 0), 2);
        line(frame, markerCenters[48], markerCenters[49], Scalar(0, 255, 0), 2);
        line(frame, markerCenters[49], markerCenters[46], Scalar(0, 255, 0), 2);

        vector<Point2f> src = {
            markerCenters[46], markerCenters[47],
            markerCenters[48], markerCenters[49]
        };
        float arenaSize = 600.0f;
        vector<Point2f> dst = {
            Point2f(0,0), Point2f(arenaSize,0),
            Point2f(arenaSize,arenaSize), Point2f(0,arenaSize)
        };
        Mat H = getPerspectiveTransform(src, dst);

        Mat topDown;
        warpPerspective(frame, topDown, H, Size(arenaSize, arenaSize));

        for (const auto& ball : balls) {
            vector<Point2f> in = {ball.position}, out;
            perspectiveTransform(in, out, H);
            if (!out.empty()) {
                circle(topDown, out[0], 15, getBallColor(ball.id), 3);
                putText(topDown, "B" + to_string(ball.id),
                        out[0] + Point2f(-10, -20), FONT_HERSHEY_SIMPLEX, 0.5,
                        getBallColor(ball.id), 2);
            }
        }

        imshow("Top-Down Arena View", topDown);
    }
}
