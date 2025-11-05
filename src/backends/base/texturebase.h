#pragma once

#ifdef USE_VITIS
#include "backends/xrt/hls/math_common.h"
#else
#include "backends/cpu/math_common.h"
#endif

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

template <class T>
T wrap(T t, AddressMode addr)
{
#pragma HLS inline

    switch (addr)
    {
    case AddressMode::Clamp:
        return clamp(t, T(0), T(1));
    case AddressMode::Repeat:
    {
        // wrap to [0,1)
        T r = fmod(t, T(1));
        if (r < T(0))
            r += T(1);
        return r;
    }
    case AddressMode::Mirror:
    {
        // mirror every [0,1], 0..1..0..
        T ip = floor(t);
        T f = t - ip;
        bool odd = static_cast<long>(ip) & 1L;
        return odd ? (T(T(1) - f)) : f;
    }
    }
    return t; // unreachable
};

template <class T, class Tex>
T nearest(Tex &tex, T y, T x, unsigned int lvl)
{
#pragma HLS inline

    const auto xi = static_cast<unsigned int>(lround(x));
    const auto yi = static_cast<unsigned int>(lround(y));
    return T(tex.texel_(yi, xi, lvl));
}

template <class T, class Tex>
T bilinear(Tex &tex, T y, T x, unsigned int lvl)
{
#pragma HLS inline

    const auto w = tex.width(lvl);
    const auto h = tex.height(lvl);

    const T xf = floor(x);
    const T yf = floor(y);
    const auto x0 = static_cast<unsigned int>(xf < T(0) ? T(0) : xf);
    const auto y0 = static_cast<unsigned int>(yf < T(0) ? T(0) : yf);
    const auto x1 = min(x0 + 1, w - 1);
    const auto y1 = min(y0 + 1, h - 1);

    const T dx = x - static_cast<T>(x0);
    const T dy = y - static_cast<T>(y0);

    const auto tl = tex.texel_(y0, x0, lvl);
    const auto tr = tex.texel_(y0, x1, lvl);
    const auto bl = tex.texel_(y1, x0, lvl);
    const auto br = tex.texel_(y1, x1, lvl);

    // if (tex.nodata() == tl || tex.nodata() == tr || tex.nodata() == bl || tex.nodata() == br)
    //     return T(tex.nodata());

    const T Cx0 = T(tl) * (T(1) - dx) + T(tr) * dx;
    const T Cx1 = T(bl) * (T(1) - dx) + T(br) * dx;
    return Cx0 * (T(1) - dy) + Cx1 * dy;
}

// Normalized sampling in [0,1] (allows outside depending on address mode)
template <class T, class Tex>
T sample(Tex &tex,
         T v, T u,
         unsigned int lvl = 0,
         AddressMode addr = AddressMode::Clamp,
         FilterMode filt = FilterMode::Bilinear)
{
#pragma HLS inline

    const T w = static_cast<T>(tex.width(lvl));
    const T h = static_cast<T>(tex.height(lvl));

    const T uu = wrap(u, addr);
    const T vv = wrap(v, addr);

    const T x = uu * w - T(0.5f);
    const T y = vv * h - T(0.5f);

    return (filt == FilterMode::Nearest)
               ? nearest<T, Tex>(tex, y, x, lvl)
               : bilinear<T, Tex>(tex, y, x, lvl);
}

template <class T, template <class> class V, class Tex>
V<T> compute_didxy(const Tex &tex, int y, int x, unsigned int lvl)
{
    // const UInt w = tex.width(lvl);
    // const UInt h = tex.height(lvl);

    int x_p = x + 1;
    int x_m = x - 1;
    int y_p = y + 1;
    int y_m = y - 1;

    if (x_p >= tex.width(lvl) || x_m < 0 || y_p >= tex.height(lvl) || y_m < 0)
    {
        return V<T>(tex.nodata(), tex.nodata(), tex.nodata());
    }

    auto f = tex.texel_(y, x, lvl);
    auto f_y_p = tex.texel_(y_p, x, lvl);
    auto f_y_m = tex.texel_(y_m, x, lvl);
    auto f_x_p = tex.texel_(y, x_p, lvl);
    auto f_x_m = tex.texel_(y, x_m, lvl);

    if (f_x_p == tex.nodata() || f_x_m == tex.nodata() ||
        f_y_p == tex.nodata() || f_y_m == tex.nodata() || f == tex.nodata())
    {
        return V<T>(tex.nodata(), tex.nodata(), tex.nodata());
    }

    V<T> out_fragment;
    out_fragment(0) = (f_x_p - f_x_m) / T(2);
    out_fragment(1) = (f_y_p - f_y_m) / T(2);
    out_fragment(2) = T(0); // f; // save the projected frame for later processing

    return out_fragment;
}

template <class T, template <class> class Tex>
void generate_mipmap(Tex<T> &tex, unsigned int lvl)
{
    const unsigned int sw = tex.width(lvl - 1);
    const unsigned int sh = tex.height(lvl - 1);
    const unsigned int dw = tex.width(lvl);
    const unsigned int dh = tex.height(lvl);

    // const auto s_idx = [&](UInt yy, UInt xx) -> T
    //{
    //     yy = min(yy, sh - 1);
    //     xx = min(xx, sw - 1);
    //     return derived_().texel_(yy, xx, lvl - 1);
    // };

    auto map_read = tex.MapRead(lvl - 1);
    auto map_write = tex.MapWrite(lvl);

    for (unsigned int y = 0; y < dh; ++y)
    {
        for (unsigned int x = 0; x < dw; ++x)
        {
            const unsigned int sx = x * 2;
            const unsigned int sy = y * 2;

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
