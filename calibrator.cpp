#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    cv::Mat image = cv::imread("path_to_your_image.jpg");
    if (image.empty()) {
        std::cout << "Could not open or find the image" << std::endl;
        return -1;
    }
    cv::imshow("Display window", image);
    cv::waitKey(0);
    return 0;
}