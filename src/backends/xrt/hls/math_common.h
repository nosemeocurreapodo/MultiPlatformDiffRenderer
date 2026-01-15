#pragma once

#include "hls_math.h"
#include "ap_int.h"
#include "ap_fixed.h"

#define num_buffers 4

#define max_width 640
#define max_height 480

#define tile_width 80
#define tile_height 60

#define max_tiles_y max_width / tile_width
#define max_tiles_x max_height / tile_height

// taken from the planet dataset
#define max_num_tri 4096 // 768;
#define max_tri_per_tile max_num_tri / (max_tiles_x * max_tiles_y * 2)

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
