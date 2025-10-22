#pragma once

#include "core/math_common.h"

template <typename T>
class BoundingBox
{
public:
    BoundingBox(T minx, T maxx, T miny, T maxy)
    {
        min_x_ = minx;
        min_y_ = miny;
        max_x_ = maxx;
        max_y_ = maxy;
    }

    template <typename Vec2Type>
    BoundingBox(Vec2Type t1, Vec2Type t2, Vec2Type t3)
    {
        min_x_ = T(min(min(t1(0), t2(0)), t3(0)));
        max_x_ = T(max(max(t1(0), t2(0)), t3(0)));
        min_y_ = T(min(min(t1(1), t2(1)), t3(1)));
        max_y_ = T(max(max(t1(1), t2(1)), t3(1)));
    }

    bool IsPixInBoundingBox(T x, T y)
    {
        if (x < min_x_ || x > max_x_ || y < min_y_ || y > max_y_)
            return false;
        return true;
    }

    template <typename Vec2Type>
    bool IsPixInBoundingBox(Vec2Type pix)
    {
        if (pix(0) < min_x_ || pix(0) > max_x_ || pix(1) < min_y_ || pix(1) > max_y_)
            return false;
        return true;
    }

    BoundingBox Union(BoundingBox win)
    {
        T min_x = min(min_x_, win.min_x);
        T max_x = max(max_x_, win.max_x);
        T min_y = min(min_y_, win.min_y);
        T max_y = max(max_y_, win.max_y);

        return BoundingBox(min_x, max_x, min_y, max_y);
    }

    BoundingBox Intersection(BoundingBox win)
    {
        T min_x = max(min_x_, win.min_x_);
        T max_x = min(max_x_, win.max_x_);
        T min_y = max(min_y_, win.min_y_);
        T max_y = min(max_y_, win.max_y_);

        return BoundingBox(min_x, max_x, min_y, max_y);
    }

    T min_x_;
    T max_x_;
    T min_y_;
    T max_y_;
};
