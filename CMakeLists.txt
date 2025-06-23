cmake_minimum_required(VERSION 3.10)
project(aruco_detector)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(aruco_detector
        main.cpp
        BotDetection.cpp
        ArenaDetection.cpp
        BallDetection.cpp
)

target_link_libraries(aruco_detector
        ${OpenCV_LIBS}
        curl
)
