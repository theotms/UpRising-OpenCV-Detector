#include <opencv2/opencv.hpp>
#include <iostream>

// Global variables for trackbar values
int lowH = 5, highH = 25;
int lowS = 100, highS = 255;
int lowV = 100, highV = 255;

// Callback function for the trackbars (does nothing, but is required)
void on_trackbar(int, void*) {}

int main() {
    // --- Camera Setup ---
    // Change the '0' if your camera is not the default one (e.g., to 1, 2, etc.)
    cv::VideoCapture cap(2);
    if (!cap.isOpened()) {
        std::cerr << "ERROR: Could not open camera" << std::endl;
        return -1;
    }

    // --- Window Setup ---
    cv::namedWindow("Original View", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("Mask View", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("Controls", cv::WINDOW_AUTOSIZE);

    // --- Create Trackbars ---
    cv::createTrackbar("Low H", "Controls", &lowH, 179, on_trackbar);
    cv::createTrackbar("High H", "Controls", &highH, 179, on_trackbar);

    cv::createTrackbar("Low S", "Controls", &lowS, 255, on_trackbar);
    cv::createTrackbar("High S", "Controls", &highS, 255, on_trackbar);

    cv::createTrackbar("Low V", "Controls", &lowV, 255, on_trackbar);
    cv::createTrackbar("High V", "Controls", &highV, 255, on_trackbar);

    std::cout << "Tuner started. Adjust sliders to isolate the orange ball." << std::endl;
    std::cout << "Press 'q' in the camera window to quit." << std::endl;

    while (true) {
        cv::Mat frame, hsv, mask;
        cap >> frame; // Grab a new frame from camera
        if (frame.empty()) break;

        // Convert frame to HSV
        cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

        // Create the mask using the current slider values
        cv::Scalar lower_bound(lowH, lowS, lowV);
        cv::Scalar upper_bound(highH, highS, highV);
        cv::inRange(hsv, lower_bound, upper_bound, mask);

        // Show the original and the mask
        cv::imshow("Original View", frame);
        cv::imshow("Mask View", mask);

        // Wait for 30 milliseconds, and break the loop if 'q' is pressed
        if (cv::waitKey(30) == 'q') {
            break;
        }
    }

    std::cout << "\n--- Final Values ---" << std::endl;
    std::cout << "Scalar lowerOrange(" << lowH << ", " << lowS << ", " << lowV << ");" << std::endl;
    std::cout << "Scalar upperOrange(" << highH << ", " << highS << ", " << highV << ");" << std::endl;

    return 0;
}