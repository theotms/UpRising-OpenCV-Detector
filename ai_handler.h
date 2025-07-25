#ifndef CAM_ARUCO_AI_HANDLER_H
#define CAM_ARUCO_AI_HANDLER_H

#include <map>
#include <string>
#include <vector>
#include <onnxruntime_cxx_api.h>
#include "world_state.h"

// This struct defines the output of our AI model
struct MovementCommand {
    float left;
    float right;
};

class AIHandler {
public:
    // Constructor takes the path to the .onnx model file
    AIHandler(const std::string& model_path);

    // Takes the current state of the world and returns movement commands
    std::map<int, MovementCommand> predictMovements(const WorldState& world);

private:
    // ONNX Runtime member variables
    Ort::Env env;
    Ort::Session session;
    Ort::AllocatorWithDefaultOptions allocator;

    // Helper function to create the 22-feature observation vector for a single bot
    std::vector<float> createObservationVector(const Bot& current_bot, const WorldState& world);
};

#endif //CAM_ARUCO_AI_HANDLER_H