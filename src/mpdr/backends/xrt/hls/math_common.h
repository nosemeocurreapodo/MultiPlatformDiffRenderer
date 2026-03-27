#pragma once

// #include "hls_math.h"
// #include "ap_int.h"
// #include "ap_fixed.h"
#include "mpdr/common/types.h"

#define NUM_BUFFERS IntType(4)

#define MAX_WIDTH IntType(640)
#define MAX_HEIGHT IntType(480)

#define NUM_TILES_Y IntType(8)
#define NUM_TILES_X IntType(8)

#define MAX_TILE_WIDTH IntType(80)
#define MAX_TILE_HEIGHT IntType(60)

// taken from the planet dataset
#define MAX_NUM_TRI 4096 // 768;
#define MAX_TRI_PER_TILE 4 * MAX_NUM_TRI / (NUM_TILES_X * NUM_TILES_Y)

inline RealType min(RealType a, RealType b)
{
    // return hls::min(a, b);
    return a < b ? a : b;
}

inline RealType max(RealType a, RealType b)
{
    // return hls::max(a, b);
    return a > b ? a : b;
}

inline RealType clamp(RealType a, RealType _min, RealType _max)
{
    // return hls::clamp(a, _min, _max);
    return min(max(a, _min), _max);
}

inline RealType floor(RealType a)
{
    // return hls::floor(a);
    return RealType(int(a));
    //   return static_cast<T>(static_cast<long>(a) - (a < 0 && a != static_cast<long>(a)));
}

inline RealType ceil(RealType a)
{
    // return hls::ceil(a);
    return RealType(int(a + (a == RealType(int(a)) ? RealType(0) : RealType(1))));
    //  return static_cast<T>(static_cast<long>(a) + (a > 0 && a != static_cast<long>(a)));
}

inline RealType lround(RealType a)
{
    // return hls::lround(a);
    return RealType(int(a + (a >= RealType(0) ? RealType(0.5) : RealType(-0.5))));
    //  return static_cast<T>(static_cast<long>(a + (a >= 0 ? 0.5 : -0.5)));
}

inline RealType fmod(RealType a, RealType b)
{
    // return hls::fmod(a, b);
    return a - b * floor(a / b);
}

inline RealType exp(RealType a)
{
    // return hls::exp(a);
    return a;
}
