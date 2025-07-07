cmake_minimum_required(VERSION 3.10)
project(aruco_detector)

set(CMAKE_CXX_STANDARD 17)

# --- Find packages ---
find_package(OpenCV REQUIRED)
find_package(PahoMqttCpp REQUIRED)

# --- Include directories ---
include_directories(
        ${OpenCV_INCLUDE_DIRS}
)

# --- EXECUTABLE 1: Your main detector program ---
add_executable(aruco_detector
        main.cpp
        BotDetection.cpp
        ArenaDetection.cpp
        BallDetection.cpp
        camera_handler.cpp
        detection_handler.cpp
        mqtt_publisher.cpp
        world_state.h
)

# --- EXECUTABLE 2: Your new tuner tool ---
add_executable(hsv_tuner
        hsv_tuner.cpp
)

# --- Link libraries for the main detector ---
target_link_libraries(aruco_detector
        ${OpenCV_LIBS}
        PahoMqttCpp::paho-mqttpp3
        curl
)

# --- Link libraries for the tuner tool ---
target_link_libraries(hsv_tuner
        ${OpenCV_LIBS}
)