#pragma once
// #include <algorithm>
// #include <cassert>
// #include <cstddef>
// #include <type_traits>
// #include <utility>
// #include <vector>
// #include <cmath> // std::floor, std::fmod

#include "core/types.h"

enum class AddressMode
{
    Repeat,
    Clamp,
    Mirror
};
enum class FilterMode
{
    Nearest,
    Bilinear
};

inline Scalar wrap(Scalar t, AddressMode addr)
{
    switch (addr)
    {
    case AddressMode::Clamp:
        return clamp(t, 0.0f, 1.0f);
    case AddressMode::Repeat:
    {
        // wrap to [0,1)
        Scalar r = fmod(t, 1.0f);
        if (r < 0.0f)
            r += 1.0f;
        return r;
    }
    case AddressMode::Mirror:
    {
        // mirror every [0,1], 0..1..0..
        Scalar ip = floor(t);
        Scalar f = t - ip;
        bool odd = static_cast<long>(ip) & 1L;
        return odd ? (1.0f - f) : f;
    }
    }
    return t; // unreachable
};

template <class T, class Tex>
T nearest(const Tex &tex, Scalar y, Scalar x, UInt lvl)
{
    const auto xi = static_cast<UInt>(lround(x));
    const auto yi = static_cast<UInt>(lround(y));
    return tex.texel_(yi, xi, lvl);
}

template <class T, class Tex>
T bilinear(const Tex &tex, Scalar y, Scalar x, UInt lvl)
{
    const auto w = tex.width(lvl);
    const auto h = tex.height(lvl);

    const Scalar xf = floor(x);
    const Scalar yf = floor(y);
    const auto x0 = static_cast<UInt>(xf < 0.0f ? 0.0f : xf);
    const auto y0 = static_cast<UInt>(yf < 0.0f ? 0.0f : yf);
    const auto x1 = min(x0 + 1, w - 1);
    const auto y1 = min(y0 + 1, h - 1);

    const Scalar dx = x - static_cast<Scalar>(x0);
    const Scalar dy = y - static_cast<Scalar>(y0);

    /*
    // auto m = MapRead(lvl); // one mapping, four reads
    const auto idx = [&](UInt yy, UInt xx)
    {
        // return m[xx + yy * w];
        //  return lvls_[lvl].buf[xx + yy * w];
        return derived_().texel_(yy, xx, lvl);
    };

    const T tl = idx(y0, x0);
    const T tr = idx(y0, x1);
    const T bl = idx(y1, x0);
    const T br = idx(y1, x1);
    */

    const T tl = tex.texel_(y0, x0, lvl);
    const T tr = tex.texel_(y0, x1, lvl);
    const T bl = tex.texel_(y1, x0, lvl);
    const T br = tex.texel_(y1, x1, lvl);

    if (tex.nodata() == tl || tex.nodata() == tr || tex.nodata() == bl || tex.nodata() == br)
        return tex.nodata();

    // const Scalar w_tl = (1.0f - dx) * (1.0f - dy);
    // const Scalar w_tr = (dx) * (1.0f - dy);
    // const Scalar w_bl = (1.0f - dx) * (dy);
    // const Scalar w_br = (dx) * (dy);
    // return static_cast<T>(tl * w_tl + tr * w_tr + bl * w_bl + br * w_br);

    const T Cx0 = tl * (Scalar(1) - dx) + tr * dx;
    const T Cx1 = bl * (Scalar(1) - dx) + br * dx;
    return static_cast<T>(Cx0 * (Scalar(1) - dy) + Cx1 * dy);
}

// Normalized sampling in [0,1] (allows outside depending on address mode)
template <class T, class Tex>
T sample(const Tex &tex,
         Scalar v, Scalar u,
         UInt lvl = 0,
         AddressMode addr = AddressMode::Clamp,
         FilterMode filt = FilterMode::Bilinear)
{
    const Scalar w = static_cast<Scalar>(tex.width(lvl));
    const Scalar h = static_cast<Scalar>(tex.height(lvl));

    const Scalar uu = wrap(u, addr);
    const Scalar vv = wrap(v, addr);

    const float x = uu * w - 0.5f;
    const float y = vv * h - 0.5f;

    return (filt == FilterMode::Nearest)
               ? nearest<T, Tex>(tex, y, x, lvl)
               : bilinear<T, Tex>(tex, y, x, lvl);
}

template <class Tex>
Vec3 compute_didxy(const Tex &tex, Scalar y, Scalar x, UInt lvl)
{
    // const UInt w = tex.width(lvl);
    // const UInt h = tex.height(lvl);

    const Int xf = Int(floor(x));
    const Int yf = Int(floor(y));

    Int x_p = xf + 1;
    Int x_m = xf - 1;
    Int y_p = yf + 1;
    Int y_m = yf - 1;

    if (x_p >= tex.width(lvl) || x_m < 0 || y_p >= tex.height(lvl) || y_m < 0)
    {
        return Vec3(tex.nodata(), tex.nodata(), tex.nodata());
    }

    Scalar f = Scalar(tex.texel_(y, x, lvl));
    Scalar f_y_p = Scalar(tex.texel_(y_p, x, lvl));
    Scalar f_y_m = Scalar(tex.texel_(y_m, x, lvl));
    Scalar f_x_p = Scalar(tex.texel_(y, x_p, lvl));
    Scalar f_x_m = Scalar(tex.texel_(y, x_m, lvl));

    if (f_x_p == tex.nodata() || f_x_m == tex.nodata() ||
        f_y_p == tex.nodata() || f_y_m == tex.nodata() || f == tex.nodata())
    {
        return Vec3(tex.nodata(), tex.nodata(), tex.nodata());
    }

    Vec3 out_fragment;
    out_fragment(0) = (f_x_p - f_x_m) / Scalar(2);
    out_fragment(1) = (f_y_p - f_y_m) / Scalar(2);
    out_fragment(2) = 0.0; // f; // save the projected frame for later processing

    return out_fragment;
}

template <class T, class Tex>
void generate_mipmap(Tex &tex, UInt lvl)
{
    const UInt sw = tex.width(lvl - 1);
    const UInt sh = tex.height(lvl - 1);
    const UInt dw = tex.width(lvl);
    const UInt dh = tex.height(lvl);

    // const auto s_idx = [&](UInt yy, UInt xx) -> T
    //{
    //     yy = min(yy, sh - 1);
    //     xx = min(xx, sw - 1);
    //     return derived_().texel_(yy, xx, lvl - 1);
    // };

    auto map_read = tex.MapRead(lvl - 1);
    auto map_write = tex.MapWrite(lvl);

    for (UInt y = 0; y < dh; ++y)
    {
        for (UInt x = 0; x < dw; ++x)
        {
            const UInt sx = x * 2;
            const UInt sy = y * 2;

            // const T tl = s_idx(sy, sx);
            // const T tr = s_idx(sy, sx + 1);
            // const T bl = s_idx(sy + 1, sx);
            // const T br = s_idx(sy + 1, sx + 1);

            const T tl = map_read[sy * sw + sx];
            const T tr = map_read[sy * sw + min(sx + 1, sw - 1)];
            const T bl = map_read[min(sy + 1, sh - 1) * sw + sx];
            const T br = map_read[min(sy + 1, sh - 1) * sw + min(sx + 1, sw - 1)];

            if (tex.nodata() == tl || tex.nodata() == tr || tex.nodata() == bl || tex.nodata() == br)
            {
                map_write[y * dw + x] = tex.nodata();
            }
            else
            {
                T val = static_cast<T>((tl + tr + bl + br) * 0.25f);
                map_write[y * dw + x] = val;
            }
        }
    }
}
