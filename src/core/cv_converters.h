#pragma once

#include <type_traits>
#include <typeindex>
#include <stdexcept>        // optional
#include <opencv2/core.hpp> // CV_32FC1 etc.
#include "core/types.h"
#include "core/typeindex_common.h"

template <typename T>
struct OpenCVFormat
{
    static_assert(dependent_false_v<T>, "OpenCVFormat<T>: unsupported type");
};

// ---- specializations ----

// float
template <>
struct OpenCVFormat<float> : std::integral_constant<int, CV_32FC1>
{
};
template <>
struct OpenCVFormat<Vec2<float>> : std::integral_constant<int, CV_32FC2>
{
};
template <>
struct OpenCVFormat<Vec3<float>> : std::integral_constant<int, CV_32FC3>
{
};
template <>
struct OpenCVFormat<Vec4<float>> : std::integral_constant<int, CV_32FC4>
{
};

// int
template <>
struct OpenCVFormat<int> : std::integral_constant<int, CV_32SC1>
{
};
template <>
struct OpenCVFormat<Vec2<int>> : std::integral_constant<int, CV_32SC2>
{
};
template <>
struct OpenCVFormat<Vec3<int>> : std::integral_constant<int, CV_32SC3>
{
};
template <>
struct OpenCVFormat<Vec4<int>> : std::integral_constant<int, CV_32SC4>
{
};

// unsigned char
template <>
struct OpenCVFormat<unsigned char> : std::integral_constant<int, CV_8UC1>
{
};
template <>
struct OpenCVFormat<Vec2<unsigned char>> : std::integral_constant<int, CV_8UC2>
{
};
template <>
struct OpenCVFormat<Vec3<unsigned char>> : std::integral_constant<int, CV_8UC3>
{
};
template <>
struct OpenCVFormat<Vec4<unsigned char>> : std::integral_constant<int, CV_8UC4>
{
};

// Compile-time API (preferred)
template <typename T>
constexpr int GetOpenCVFormat()
{
    return OpenCVFormat<T>::value;
}
