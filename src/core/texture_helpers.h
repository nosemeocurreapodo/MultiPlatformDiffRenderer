#pragma once

#include <opencv2/opencv.hpp>

cv::Mat MakeCheckerTex(int w = 512, int h = 512, int checker = 32, int channels = 3)
{
    cv::Mat tex;

    if (channels == 3)
        tex = cv::Mat(h, w, CV_32FC3);
    else
        tex = cv::Mat(h, w, CV_32FC1);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            bool c = ((x / checker) + (y / checker)) & 1;
            if (channels == 3)
                tex.at<cv::Vec3f>(y, x) = c ? cv::Vec3f(0.5, 0.5, 0.5) : cv::Vec3f(1.0, 1.0, 1.0);
            else
                tex.at<float>(y, x) = c ? 0.5f : 1.0f;
        }
    }
    return tex;
}