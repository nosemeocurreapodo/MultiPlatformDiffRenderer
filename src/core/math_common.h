#pragma once

template <typename T>
inline T min(T a, T b)
{
    // return std::min(a, b);
    return a < b ? a : b;
}

template <typename T>
inline T max(T a, T b)
{
    // return std::max(a, b);
    return a > b ? a : b;
}

template <typename T>
inline T clamp(T a, T _min, T _max)
{
    // return std::clamp(a, _min, _max);
    return min(max(a, _min), _max);
}

template <typename T>
inline T floor(T a)
{
    // return std::floor(a);
    return T(int(a));
    //  return static_cast<T>(static_cast<long>(a) - (a < 0 && a != static_cast<long>(a)));
}

template <typename T>
inline T ceil(T a)
{
    // return std::ceil(a);
    return T(int(a + (a == int(a) ? 0 : 1)));
    // return static_cast<T>(static_cast<long>(a) + (a > 0 && a != static_cast<long>(a)));
}

template <typename T>
inline T lround(T a)
{
    // return std::lround(a);
    return T(int(a + (a >= 0 ? 0.5 : -0.5)));
    // return static_cast<T>(static_cast<long>(a + (a >= 0 ? 0.5 : -0.5)));
}

template <typename T>
inline T fmod(T a, T b)
{
    // return std::fmod(a, b);
    return a - b * floor(a / b);
}
