#include "bot_detector.h"

using namespace cv;
using namespace std;

vector<DetectedBot> detectBots(const Mat& frame, Mat& displayFrame, const Mat& cameraMatrix, const Mat& distCoeffs, float markerLength) {
    vector<int> ids;
    vector<vector<Point2f>> corners;
    auto dictionary = aruco::getPredefinedDictionary(aruco::DICT_4X4_50);
    aruco::detectMarkers(frame, dictionary, corners, ids);

    vector<DetectedBot> found_bots;

    if (!ids.empty()) {
        // Draw all detected markers onto the display frame
        aruco::drawDetectedMarkers(displayFrame, corners, ids);

        vector<Vec3d> rvecs, tvecs;
        aruco::estimatePoseSingleMarkers(corners, markerLength, cameraMatrix, distCoeffs, rvecs, tvecs);

        for (int i = 0; i < ids.size(); ++i) {
            // Filter for bot markers only (IDs less than 46)
            if (ids[i] < 46) {
                // Draw axis on the display frame
                aruco::drawAxis(displayFrame, cameraMatrix, distCoeffs, rvecs[i], tvecs[i], markerLength * 0.5f);

                Point2f center = (corners[i][0] + corners[i][1] + corners[i][2] + corners[i][3]) / 4;

                Point2f top_mid = (corners[i][0] + corners[i][1]) / 2;
                Point2f bottom_mid = (corners[i][2] + corners[i][3]) / 2;
                float angleRad = atan2(top_mid.y - bottom_mid.y, top_mid.x - bottom_mid.x);
                float angleDeg = angleRad * 180.0 / CV_PI;

                // Draw the orientation line on the display frame
                line(displayFrame, bottom_mid, top_mid, Scalar(0, 255, 0), 2);

                found_bots.push_back({ids[i], center, angleDeg, true});
            }
        }
    }
    return found_bots;
}