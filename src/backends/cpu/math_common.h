#pragma once

#include <cmath>
#include <algorithm>

template <typename T>
inline T min(T a, T b)
{
    return std::min(a, b);
}

template <typename T>
inline T max(T a, T b)
{
    return std::max(a, b);
}

template <typename T>
inline T clamp(T a, T _min, T _max)
{
    //return std::clamp(a, _min, _max);
    return std::min(std::max(a, _min), _max);
}

template <typename T>
inline T floor(T a)
{
    return std::floor(a);
}

template <typename T>
inline T ceil(T a)
{
    return std::ceil(a);
}

template <typename T>
inline T lround(T a)
{
    return std::lround(a);
}

template <typename T>
inline T fmod(T a, T b)
{
    return std::fmod(a, b);
}

template <typename T>
inline T exp(T a)
{
    return std::exp(a);
}

template <typename T>
inline T fabs(T a)
{
    return std::fabs(a);
}