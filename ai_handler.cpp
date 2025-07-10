#include "ai_handler.h"
#include <iostream>
#include <vector>

// These constants define the coordinate system of your top-down map
constexpr int ARENA_WIDTH = 480;
constexpr int ARENA_HEIGHT = 480;

AIHandler::AIHandler(const std::string& model_path)
    // This is an initializer list that sets up the ONNX environment and session
    : env(ORT_LOGGING_LEVEL_WARNING, "RobotSoccerAI"),
      session(env, model_path.c_str(), Ort::SessionOptions{nullptr}) {
    std::cout << "[AI] ONNX model loaded successfully from: " << model_path << std::endl;
}

std::map<int, MovementCommand> AIHandler::predictMovements(const WorldState& world) {
    std::map<int, MovementCommand> commands;
    if (world.bots.empty()) {
        return commands; // Return empty commands if no bots are detected
    }

    // --- Prepare Input Data for the AI ---
    std::vector<float> input_data;
    std::vector<const Bot*> bot_order; // Keep track of bot order to map outputs correctly

    for (const auto& bot : world.bots) {
        // The AI model expects normalized coordinates (from 0.0 to 1.0)
        input_data.push_back(bot.center.x / ARENA_WIDTH);
        input_data.push_back(bot.center.y / ARENA_HEIGHT);
        bot_order.push_back(&bot);
    }

    // --- Create the ONNX Input Tensor ---
    std::vector<int64_t> input_shape = {static_cast<int64_t>(world.bots.size()), 2};
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_data.data(), input_data.size(),
        input_shape.data(), input_shape.size());

    // --- Run Inference ---
    // *** CORRECTED API CALLS FOR NEW ONNX RUNTIME VERSION ***
    // The new API uses smart pointers (AllocatedStringPtr) to manage memory automatically.
    Ort::AllocatedStringPtr input_name_ptr = session.GetInputNameAllocated(0, allocator);
    Ort::AllocatedStringPtr output_name_ptr = session.GetOutputNameAllocated(0, allocator);
    const char* input_names[] = {input_name_ptr.get()};
    const char* output_names[] = {output_name_ptr.get()};

    try {
        auto output_tensors = session.Run(Ort::RunOptions{nullptr}, input_names, &input_tensor, 1, output_names, 1);

        // --- Process the AI's Output ---
        float* outputs = output_tensors[0].GetTensorMutableData<float>();
        for (size_t i = 0; i < bot_order.size(); ++i) {
            int bot_id = bot_order[i]->id;
            // The output is a flat array [left1, right1, left2, right2, ...]
            commands[bot_id] = {outputs[i * 2], outputs[i * 2 + 1]};
        }

    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime inference error: " << e.what() << std::endl;
    }

    // No need to manually free memory; the AllocatedStringPtr handles it.
    return commands;
}