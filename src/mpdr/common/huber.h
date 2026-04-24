#pragma once

#include "mpdr/common/types.h"

#ifdef USE_VITIS
#include "mpdr/backends/xrt/hls/math_common.h"
#else
#include "mpdr/backends/cpu/math_common.h"
#endif

static inline RealType huber_weight(RealType r, RealType thresh) noexcept
{
    const RealType a = abs(r);
    if (a <= thresh || a == RealType(0.0))
        return RealType(1);
    return thresh / a;
}