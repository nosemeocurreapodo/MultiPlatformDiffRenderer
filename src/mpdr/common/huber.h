#pragma once

#include "mpdr/common/types.h"

static inline RealType huber_weight(RealType r, RealType thresh) noexcept
{
    const RealType a = fabs(r);
    if (a <= thresh || a == RealType(0.0))
        return RealType(1);
    return thresh / a;
}