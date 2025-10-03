#pragma once

#include "backends/base/texturebase.h"
#include "backends/xrt/hls/bufferhls.h"
#include "core/types.h"

template <class T>
class TextureHLS : public TextureBase<TextureHLS<T>, T>
{
public:
    // using value_type = T;
    // using size_type = std::size_t;

    // Default-construct an empty texture. Safe to assign later.
    TextureHLS() = default;

    TextureHLS(UInt w, UInt h, UInt size, T nodata, T *base)
        : nodata_(nodata), storage_(size, base)
    {
        build_pyramid_(w, h);
    }

    // Rule of 5
    TextureHLS(const TextureHLS &) = default;
    TextureHLS &operator=(const TextureHLS &) = default;
    TextureHLS(TextureHLS &&) noexcept = default;
    TextureHLS &operator=(TextureHLS &&) noexcept = default;
    ~TextureHLS() = default;

    // Introspection
    UInt width(UInt lvl) const { return levels_[lvl].w; }
    UInt height(UInt lvl) const { return levels_[lvl].h; }
    UInt levels() const { return n_levels_; }
    UInt size(int lvl) const { return width(lvl) * height(lvl); }
    UInt type_size() const { return sizeof(T); };
    T nodata() const { return nodata_; }

    // Fill a level with a constant
    void fill(UInt lvl, const T &v)
    {
        for (int i = 0; i < size(lvl); i++)
        {
            storage_.data()[i] = v;
        }
    }

    // Read/Write a single texel (bounds-checked in debug)
    T texel_(UInt y, UInt x, UInt lvl) const
    {
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif
        const auto &L = levels_[lvl];
        return storage_.data()[L.offset + y * L.w + x];
    }

    void set_texel_(const T &v, UInt y, UInt x, UInt lvl)
    {
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif
        const auto &L = levels_[lvl];
        storage_.data()[L.offset + y * L.w + x] = v;
    }

protected:
    template <class Mesh, template <class> class Texture>
    friend class DepthRendererBase;
    template <class Mesh, template <class> class Texture>
    friend class ImageRendererBase;
    // friend class DepthRendererCPU;
    // friend class ImageRendererCPU;
    // friend class ResidualRendererCPU;
    // friend class L2RendererCPU;
    // friend class DIDxyRendererCPU;
    // friend class JPoseRendererCPU;
    // friend class JMapRendererCPU;

    struct Level
    {
        UInt offset; // element offset in storage_
        UInt size;   // elements at this level (w*h*channels)
        UInt w, h;
        // optional: std::size_t pitch; // elements per row if you pad rows
    };

    Level levels_[15];
    UInt n_levels_;

    BufferHLS<T> storage_;

    T nodata_;

    void build_pyramid_(UInt w, UInt h)
    {
        if (w == 0 || h == 0)
            return;

        UInt running = 0;
        // build until 1x1 (inclusive)
        n_levels_ = 0;
        while (true)
        {
            // levels_.push_back(Level{w, h, BufferCPU<T>(w * h)});
            Level L;
            L.w = w;
            L.h = h;
            L.size = w * h;
            L.offset = running;

            levels_[n_levels_] = L;
            running += L.size;
            n_levels_++;

            if (w == 1 && h == 1)
                break;

            w = max<UInt>(1, w >> 1);
            h = max<UInt>(1, h >> 1);
        }
    }
};
