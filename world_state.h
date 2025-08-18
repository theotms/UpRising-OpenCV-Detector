#ifndef WORLD_STATE_H
#define WORLD_STATE_H

#include <vector>
#include <opencv2/core.hpp>
#include "ball_detector.h"
#include "json.hpp"

using json = nlohmann::json;

struct Bot {
    int id;
    cv::Point2f center;
    float angle;
    bool is_ai;
};

struct WorldState {
    std::vector<Bot> bots;
    std::vector<Ball> balls; // MODIFIED: Now stores a vector of balls
};

// JSON serialization functions (no changes needed here)
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
            {"balls", w.balls} // MODIFIED: Serializes the vector of balls
    };
}

#endif // WORLD_STATE_H