#pragma once

#include "core/types.h"

#ifdef USE_VITIS
#include "backends/xrt/hls/math_common.h"
#else
#include "backends/cpu/math_common.h"
#endif

static inline RealType huber_weight(RealType r, RealType thresh) noexcept
{
    const RealType a = fabs(r);
    if (a <= thresh || a == 0.0f)
        return 1.0f;
    return thresh / a;
}