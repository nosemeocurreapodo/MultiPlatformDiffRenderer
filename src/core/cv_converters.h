#pragma once

#include <typeindex>
#include <typeinfo>
#include <opencv2/opencv.hpp>

inline int GetOpenCVFormat(std::type_index tindex, int channels)
{
    static std::map<std::pair<std::type_index, int>, int> opencv_format_map;

    opencv_format_map[std::make_pair(GetTypeIndex<float>(), 1)] = CV_32FC1;
    opencv_format_map[std::make_pair(GetTypeIndex<float>(), 2)] = CV_32FC2;
    opencv_format_map[std::make_pair(GetTypeIndex<float>(), 3)] = CV_32FC3;
    opencv_format_map[std::make_pair(GetTypeIndex<float>(), 4)] = CV_32FC4;

    opencv_format_map[std::make_pair(GetTypeIndex<int>(), 1)] = CV_32SC1;
    opencv_format_map[std::make_pair(GetTypeIndex<int>(), 2)] = CV_32SC2;
    opencv_format_map[std::make_pair(GetTypeIndex<int>(), 3)] = CV_32SC3;
    opencv_format_map[std::make_pair(GetTypeIndex<int>(), 4)] = CV_32SC4;

    opencv_format_map[std::make_pair(GetTypeIndex<unsigned char>(), 1)] = CV_8UC1;
    opencv_format_map[std::make_pair(GetTypeIndex<unsigned char>(), 2)] = CV_8UC2;
    opencv_format_map[std::make_pair(GetTypeIndex<unsigned char>(), 3)] = CV_8UC3;
    opencv_format_map[std::make_pair(GetTypeIndex<unsigned char>(), 4)] = CV_8UC4;

    return opencv_format_map[std::make_pair(tindex, channels)];
}
