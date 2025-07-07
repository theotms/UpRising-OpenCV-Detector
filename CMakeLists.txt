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

# --- Source files ---
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

# file(GLOB HEADER_FILES *.h)

# --- Link libraries ---
target_link_libraries(aruco_detector
        ${OpenCV_LIBS}
        PahoMqttCpp::paho-mqttpp3
        curl
)
