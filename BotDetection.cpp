#include "bot_detector.h"

using namespace cv;
using namespace std;

vector<DetectedBot> detectBots(const Mat& frame, Mat& displayFrame, const Mat& cameraMatrix, const Mat& distCoeffs, float markerLength) {
    vector<int> ids;
    vector<vector<Point2f>> corners;

    // --- FIX: Use the modern ArucoDetector API ---
    // 1. Get the dictionary object
    cv::aruco::Dictionary dictionary = aruco::getPredefinedDictionary(aruco::DICT_4X4_50);
    // 2. Create an ArucoDetector instance
    cv::aruco::ArucoDetector detector(dictionary);
    // 3. Use the detector to find markers
    detector.detectMarkers(frame, corners, ids);
    // --- END FIX ---

    vector<DetectedBot> found_bots;

    if (!ids.empty()) {
        aruco::drawDetectedMarkers(displayFrame, corners, ids);

        vector<Vec3d> rvecs, tvecs;
        aruco::estimatePoseSingleMarkers(corners, markerLength, cameraMatrix, distCoeffs, rvecs, tvecs);

        for (size_t i = 0; i < ids.size(); ++i) {
            if (ids[i] < 46) {
                Point2f center = (corners[i][0] + corners[i][1] + corners[i][2] + corners[i][3]) / 4;

                Point2f top_mid = (corners[i][0] + corners[i][1]) / 2;
                Point2f bottom_mid = (corners[i][2] + corners[i][3]) / 2;
                float angleRad = atan2(top_mid.y - bottom_mid.y, top_mid.x - bottom_mid.x);
                float angleDeg = angleRad * 180.0 / CV_PI;

                line(displayFrame, bottom_mid, top_mid, Scalar(0, 255, 0), 2);

                found_bots.push_back({ids[i], center, angleDeg, true});
            }
        }
    }
    return found_bots;
}