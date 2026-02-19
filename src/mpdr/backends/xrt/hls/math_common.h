#pragma once

#include "hls_math.h"
#include "ap_int.h"
#include "ap_fixed.h"

#define NUM_BUFFERS 4

#define MAX_WIDTH 640
#define MAX_HEIGHT 480

#define NUM_TILES_Y 8
#define NUM_TILES_X 8

#define MAX_TILE_WIDTH 80
#define MAX_TILE_HEIGHT 60

// taken from the planet dataset
#define MAX_NUM_TRI 4096 // 768;
#define MAX_TRI_PER_TILE 4 * MAX_NUM_TRI / (NUM_TILES_X * NUM_TILES_Y)

template <typename T>
inline T min(T a, T b)
{
    return hls::min(a, b);
    // return a < b ? a : b;
}

template <typename T>
inline T max(T a, T b)
{
    return hls::max(a, b);
    // return a > b ? a : b;
}

template <typename T>
inline T clamp(T a, T _min, T _max)
{
    // return hls::clamp(a, _min, _max);
    return min(max(a, _min), _max);
}

template <typename T>
inline T floor(T a)
{
    return hls::floor(a);
    // return T(int(a));
    //   return static_cast<T>(static_cast<long>(a) - (a < 0 && a != static_cast<long>(a)));
}

template <typename T>
inline T ceil(T a)
{
    return hls::ceil(a);
    // return T(int(a + (a == int(a) ? 0 : 1)));
    //  return static_cast<T>(static_cast<long>(a) + (a > 0 && a != static_cast<long>(a)));
}

template <typename T>
inline T lround(T a)
{
    return hls::lround(a);
    // return T(int(a + (a >= 0 ? T(0.5) : T(-0.5))));
    //  return static_cast<T>(static_cast<long>(a + (a >= 0 ? 0.5 : -0.5)));
}

template <typename T>
inline T fmod(T a, T b)
{
    return hls::fmod(a, b);
    // return a - b * floor(a / b);
}

template <typename T>
inline T exp(T a)
{
    return hls::exp(a);
}
