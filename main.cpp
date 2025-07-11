#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <thread>
#include "detection_handler.h" // Contains SharedState and loop declarations

using namespace cv;
using namespace std;
using json = nlohmann::json;

int main() {
    // --- 1. Load Calibration Data ---
    ifstream file("rpi-camera-calib-params.json");
    if (!file.is_open()) {
        cerr << "ERROR: Calibration file 'rpi-camera-calib-params.json' not found!" << endl;
        return -1;
    }

    json calib;
    try {
        file >> calib;
    } catch (json::parse_error& e) {
        cerr << "ERROR: Failed to parse calibration JSON: " << e.what() << endl;
        return -1;
    }

    // --- CORRECTED MATRIX LOADING ---
    // This handles the nested array structure for the camera matrix.
    Mat cameraMatrix(3, 3, CV_32F);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            cameraMatrix.at<float>(i, j) = calib["mtx"][i][j];
        }
    }

    // --- CORRECTED DISTORTION COEFFICIENT LOADING ---
    // Your 'dist' array is flat, not nested. This removes the extra index.
    // We also read all 14 coefficients from your file, as OpenCV can handle them.
    Mat distCoeffs(1, 14, CV_32F);
    for (int i = 0; i < 14; i++) {
        distCoeffs.at<float>(0, i) = calib["dist"][i];
    }

    // Set the physical size of your ArUco markers (in meters)
    float markerLength = 0.03f; // Example: 3cm markers

    // --- 2. Initialize Camera ---
    VideoCapture cap("/dev/video2", CAP_V4L2);
    if (!cap.isOpened()) {
        cerr << "ERROR: Failed to open camera on /dev/video2." << endl;
        return -1;
    }
    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);

    // --- 3. Start Processing Threads ---
    SharedState state; // This object is shared between the two threads

    cout << "Starting camera and detection threads..." << endl;
    thread camThread(captureLoop, std::ref(cap), std::ref(state));
    thread detectThread(detectionLoop, std::ref(cameraMatrix), std::ref(distCoeffs), markerLength, std::ref(state));

    // --- 4. Wait for Threads to Complete ---
    // The main thread will wait here until the user presses 'q' in the display window.
    camThread.join();
    detectThread.join();

    cout << "Application finished." << endl;
    return 0;
}