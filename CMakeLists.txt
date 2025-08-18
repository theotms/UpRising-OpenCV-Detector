cmake_minimum_required(VERSION 3.10)
project(aruco_detector)

set(CMAKE_CXX_STANDARD 17)

# --- Find Dependencies ---
find_package(OpenCV REQUIRED)

# --- Find Paho MQTT Libraries ---
set(PAHO_MQTT_INCLUDE_DIR "/usr/local/include")
set(PAHO_MQTT_LIB_DIR "/usr/local/lib")

find_library(PAHO_MQTT_CPP_LIBRARY NAMES paho-mqttpp3 HINTS ${PAHO_MQTT_LIB_DIR})
if (NOT PAHO_MQTT_CPP_LIBRARY)
    message(FATAL_ERROR "Paho MQTT C++ library (paho-mqttpp3) not found in ${PAHO_MQTT_LIB_DIR}")
endif()

find_library(PAHO_MQTT_C_LIBRARY NAMES paho-mqtt3as HINTS ${PAHO_MQTT_LIB_DIR})
if (NOT PAHO_MQTT_C_LIBRARY)
    message(FATAL_ERROR "Paho MQTT C library (paho-mqtt3as) not found in ${PAHO_MQTT_LIB_DIR}")
endif()

# --- Find ONNX Runtime Library ---
set(ONNXRUNTIME_DIR /home/koneauto6/Documents/Ball_Version/onnxruntime-linux-x64-1.18.1)
link_directories(${ONNXRUNTIME_DIR}/lib)

# --- Define Executables ---
add_executable(aruco_detector
        main.cpp
        camera_handler.cpp
        detection_handler.cpp
        ArenaDetection.cpp
        BotDetection.cpp
        BallDetection.cpp
        mqtt_publisher.cpp
        ai_handler.cpp)

# ✅ HSV tuner executable (assume hsv_tuner.cpp exists in your project)
add_executable(hsv_tuner
        hsv_tuner.cpp)

# --- Configure Includes ---
target_include_directories(aruco_detector PUBLIC
        ${OpenCV_INCLUDE_DIRS}
        ${PAHO_MQTT_INCLUDE_DIR}
        ${ONNXRUNTIME_DIR}/include
)

target_include_directories(hsv_tuner PUBLIC
        ${OpenCV_INCLUDE_DIRS})

# --- Link Libraries ---
target_link_libraries(aruco_detector
        ${OpenCV_LIBS}
        ${PAHO_MQTT_CPP_LIBRARY}
        ${PAHO_MQTT_C_LIBRARY}
        onnxruntime)

target_link_libraries(hsv_tuner
        ${OpenCV_LIBS})