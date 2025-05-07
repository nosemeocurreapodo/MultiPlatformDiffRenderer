#pragma once

#include "core/types.h"
#include "core/common.h"

template <typename Type>
class BoundingBox
{
public:
    BoundingBox(Type minx, Type maxx, Type miny, Type maxy)
    {
        min_x_ = minx;
        min_y_ = miny;
        max_x_ = maxx;
        max_y_ = maxy;
    }

    BoundingBox(Vec2 t1, Vec2 t2, Vec2 t3)
    {
        min_x_ = Type(min(min(t1(0), t2(0)), t3(0)));
        max_x_ = Type(max(max(t1(0), t2(0)), t3(0)));
        min_y_ = Type(min(min(t1(1), t2(1)), t3(1)));
        max_y_ = Type(max(max(t1(1), t2(1)), t3(1)));
    }

    bool IsPixInBoundingBox(Type x, Type y)
    {
        if (x < min_x_ || x > max_x_ || y < min_y_ || y > max_y_)
            return false;
        return true;
    }

    bool IsPixInBoundingBox(Vec2 pix)
    {
        if (pix(0) < min_x_ || pix(0) > max_x_ || pix(1) < min_y_ || pix(1) > max_y_)
            return false;
        return true;
    }

    BoundingBox Union(BoundingBox win)
    {
        Type min_x = min(min_x_, win.min_x);
        Type max_x = max(max_x_, win.max_x);
        Type min_y = min(min_y_, win.min_y);
        Type max_y = max(max_y_, win.max_y);

        return BoundingBox(min_x, max_x, min_y, max_y);
    }

    BoundingBox Intersection(BoundingBox win)
    {
        Type min_x = max(min_x_, win.min_x_);
        Type max_x = min(max_x_, win.max_x_);
        Type min_y = max(min_y_, win.min_y_);
        Type max_y = min(max_y_, win.max_y_);

        return BoundingBox(min_x, max_x, min_y, max_y);
    }

    Type min_x_;
    Type max_x_;
    Type min_y_;
    Type max_y_;
};
