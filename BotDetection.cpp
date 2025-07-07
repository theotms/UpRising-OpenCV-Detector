#include "bot_detector.h"
#include "json.hpp"

using json = nlohmann::json;
using namespace cv;
using namespace std;
using namespace aruco;

vector<DetectedBot> detectBots(Mat& frame, const Mat& cameraMatrix, const Mat& distCoeffs, float markerLength) {
    vector<DetectedBot> detected;

    Ptr<Dictionary> dictionary = getPredefinedDictionary(DICT_4X4_50);
    vector<int> ids;
    vector<vector<Point2f>> corners;

    detectMarkers(frame, dictionary, corners, ids);
    if (ids.empty()) return detected;

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

            // Compute angle from vector between two corners
            Point2f dir = corners[i][0] - corners[i][1];
            float angle = atan2(dir.y, dir.x) * 180.0 / CV_PI;

            DetectedBot bot;
            bot.id = id;
            bot.center = center;
            bot.angleDeg = angle;
            bot.isAI = (id >= 3);

            detected.push_back(bot);

            Scalar color = bot.isAI ? Scalar(0, 0, 255) : Scalar(255, 255, 0);
            string label = (bot.isAI ? "AI " : "Player ") + to_string(id);
            putText(frame, label, center + Point2f(5, -5), FONT_HERSHEY_SIMPLEX, 0.6, color, 2);
            circle(frame, center, 6, color, FILLED);
        }
    }

    return detected;
}
