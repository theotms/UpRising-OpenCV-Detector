#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <thread>

#include "detection_handler.h"
#include "json.hpp"
using json = nlohmann::json;


using namespace cv;
using namespace std;
using json = nlohmann::json;

int main() {
    ifstream file("rpi-camera-calib-params.json");
    if (!file) {
        cerr << "Calibration file not found!" << endl;
        return -1;
    }

    json calib;
    file >> calib;
    Mat cameraMatrix(3, 3, CV_32F);
    Mat distCoeffs(1, 5, CV_32F);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            cameraMatrix.at<float>(i, j) = calib["mtx"][i][j];
    for (int i = 0; i < 5; i++)
        distCoeffs.at<float>(0, i) = calib["dist"][i];

    float markerLength = 0.15f;

    VideoCapture cap("/dev/video0", CAP_V4L2);
    if (!cap.isOpened()) {
        cerr << "Failed to open camera." << endl;
        return -1;
    }
    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);

    SharedState state;

    thread camThread(captureLoop, std::ref(cap), std::ref(state));
    thread detectThread(detectionLoop, std::ref(cameraMatrix), std::ref(distCoeffs), markerLength, std::ref(state));

    camThread.join();
    detectThread.join();

    return 0;
}
