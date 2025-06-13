#include "bot_detector.h"

using namespace cv;
using namespace std;
using namespace aruco;

void detectBots(Mat& frame, const Mat& cameraMatrix, const Mat& distCoeffs, float markerLength) {
    Ptr<Dictionary> dictionary = getPredefinedDictionary(DICT_4X4_50);
    vector<int> ids;
    vector<vector<Point2f>> corners;

    detectMarkers(frame, dictionary, corners, ids);
    if (ids.empty()) return;

    vector<Point3f> objectPoints = {
        {-markerLength/2,  markerLength/2, 0},
        { markerLength/2,  markerLength/2, 0},
        { markerLength/2, -markerLength/2, 0},
        {-markerLength/2, -markerLength/2, 0}
    };

    for (size_t i = 0; i < ids.size(); ++i) {
        int id = ids[i];

        if (id >= 1 && id <= 4 && corners[i].size() == 4) {
            Point2f center(0, 0);
            for (const auto& pt : corners[i]) center += pt;
            center *= 0.25f;

            if (center.x > 0 && center.x < frame.cols &&
                center.y > 0 && center.y < frame.rows) {

                Scalar color = (id <= 2) ? Scalar(255, 255, 0) : Scalar(0, 0, 255);
                string label = (id <= 2 ? "Player " : "AI ") + to_string(id);

                circle(frame, center, 6, color, FILLED);
                putText(frame, label, center + Point2f(5, -5), FONT_HERSHEY_SIMPLEX, 0.6, color, 2);

                // Estimate pose, but don't draw axes
                Vec3d rvec, tvec;
                solvePnP(objectPoints, corners[i], cameraMatrix, distCoeffs, rvec, tvec);
                }
        }
    }
}
