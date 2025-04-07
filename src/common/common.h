#pragma once

#include <typeindex>
#include <typeinfo>
#include <opencv2/opencv.hpp>

static bool IsTriangleEqual(Vec3i tri_indices_1, Vec3i tri_indices_2)
{
    bool isIndicePresent[3];
    for (int tri_indice = 0; tri_indice < 3; tri_indice++)
    {
        isIndicePresent[tri_indice] = false;
        if (tri_indices_1(tri_indice) == tri_indices_2(0) || tri_indices_1(tri_indice) == tri_indices_2(1) || tri_indices_1(tri_indice) == tri_indices_2(2))
            isIndicePresent[tri_indice] = true;
    }
    if (isIndicePresent[0] && isIndicePresent[1] && isIndicePresent[2])
        return true;
    return false;
}

static bool IsEdgeEqual(Vec2i edge_indices_1, Vec2i edge_indices_2)
{
    bool isIndicePresent[2];
    for (int edge_indice = 0; edge_indice < 2; edge_indice++)
    {
        isIndicePresent[edge_indice] = false;
        if (edge_indices_1(edge_indice) == edge_indices_2(0) || edge_indices_1(edge_indice) == edge_indices_2(1))
            isIndicePresent[edge_indice] = true;
    }
    if (isIndicePresent[0] && isIndicePresent[1])
        return true;
    return false;
}

inline std::vector<Vec2> UniformTexCoords(int width, int height)
{
    std::vector<Vec2> texcoords;
    for (float y = 0.0; y < height; y++)
    {
        for (float x = 0.0; x < width; x++)
        {
            Vec2 pix;
            pix(0) = x / (width - 1);
            pix(1) = y / (height - 1);

            texcoords.push_back(pix);
        }
    }

    return texcoords;
}

inline float randomDepth(float min_depth, float max_depth)
{
    float depth = (max_depth - min_depth) * float(rand() % 1000) / 1000.0 + min_depth;
    return depth;
}

inline float verticallySmoothDepth(Vec2 pix, float min_depth, float max_depth)
{
    // max depth when y = 0
    float depth = max_depth + (min_depth - max_depth) * pix(1);
    return depth;
}

template <typename T>
inline std::type_index GetTypeIndex()
{
    return std::type_index(typeid(T));
}

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
