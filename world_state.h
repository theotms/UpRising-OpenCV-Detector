#ifndef WORLD_STATE_H
#define WORLD_STATE_H

#include <vector>
#include <opencv2/core.hpp>
#include "ball_detector.h"  // uses Ball with fields: center (cv::Point2f), radius, id

struct Bot {
    int id;
    cv::Point2f center;
    float angle;
    bool is_ai;
};

struct WorldState {
    std::vector<Bot> bots;
    Ball ball;
};

#include "json.hpp"
using json = nlohmann::json;

inline void to_json(json& j, const Ball& b) {
    j = json{
            {"center", {b.center.x, b.center.y}},
            {"radius", b.radius}
    };
}

inline void to_json(json& j, const Bot& b) {
    j = json{
            {"id", b.id},
            {"center", {b.center.x, b.center.y}},
            {"angle", b.angle},
            {"is_ai", b.is_ai}
    };
}

inline void to_json(json& j, const WorldState& w) {
    j = json{
            {"bots", w.bots},
            {"ball", w.ball}
    };
}

#endif // WORLD_STATE_H
