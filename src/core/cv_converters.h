#pragma once

#include <opencv2/opencv.hpp>
#include "core/typeindex_common.h"
#include "core/types.h"

int GetOpenCVFormat(std::type_index tindex)
{
    static std::map<std::type_index, int> opencv_format_map;

    opencv_format_map[GetTypeIndex<float>()] = CV_32FC1;
    opencv_format_map[GetTypeIndex<Vec2<float>>()] = CV_32FC2;
    opencv_format_map[GetTypeIndex<Vec3<float>>()] = CV_32FC3;
    opencv_format_map[GetTypeIndex<Vec4<float>>()] = CV_32FC4;

    opencv_format_map[GetTypeIndex<int>()] = CV_32SC1;
    opencv_format_map[GetTypeIndex<Vec2<int>>()] = CV_32SC2;
    opencv_format_map[GetTypeIndex<Vec3<int>>()] = CV_32SC3;
    opencv_format_map[GetTypeIndex<Vec4<int>>()] = CV_32SC4;

    opencv_format_map[GetTypeIndex<unsigned char>()] = CV_8UC1;
    opencv_format_map[GetTypeIndex<Vec2<unsigned char>>()] = CV_8UC2;
    opencv_format_map[GetTypeIndex<Vec3<unsigned char>>()] = CV_8UC3;
    opencv_format_map[GetTypeIndex<Vec4<unsigned char>>()] = CV_8UC4;

    return opencv_format_map[tindex];
}
