#pragma once

#include "backends/base/texturebase.h"
#include "backends/xrt/hls/bufferhls.h"
#include "core/types.h"

template <class T>
class TextureRAM : public TextureBase<TextureRAM<T>, T>
{
public:
    TextureRAM() = default;

    TextureRAM(UInt w, UInt h, T nodata, T *base)
        : nodata_(nodata), storage_(int(w * h * (1.0 + 1.0 / 4.0 + 1.0 / 8.0)), base)
    {
        build_pyramid_(w, h);
    }

    TextureRAM(UInt w, UInt h, T nodata)
        : nodata_(nodata), storage_(int(w * h * (1.0 + 1.0 / 4.0 + 1.0 / 8.0)))
    {
        build_pyramid_(w, h);
    }

    // Rule of 5
    TextureRAM(const TextureRAM &) = default;
    TextureRAM &operator=(const TextureRAM &) = default;
    TextureRAM(TextureRAM &&) noexcept = default;
    TextureRAM &operator=(TextureRAM &&) noexcept = default;
    ~TextureRAM() = default;

    // Introspection
    UInt width(UInt lvl) const { return levels_[lvl].w; }
    UInt height(UInt lvl) const { return levels_[lvl].h; }
    UInt levels() const { return n_levels_; }
    UInt size(UInt lvl) const { return width(lvl) * height(lvl); }
    UInt type_size() const { return sizeof(T); };
    T nodata() const { return nodata_; }

    // Fill a level with a constant
    void fill(UInt lvl, const T &v)
    {
    texturehls_fill_loop:
        for (int i = 0; i < size(lvl); i++)
        {
            storage_[levels_[lvl].offset + i] = v;
        }
    }

    // Read/Write a single texel (bounds-checked in debug)
    T texel_(UInt y, UInt x, UInt lvl) const
    {
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif
        return storage_[levels_[lvl].offset + y * levels_[lvl].w + x];
    }

    void set_texel_(const T &v, UInt y, UInt x, UInt lvl)
    {
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif
        storage_[levels_[lvl].offset + y * levels_[lvl].w + x] = v;
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

    BufferRAM<T> storage_;

    T nodata_;

    void build_pyramid_(UInt w, UInt h)
    {
        if (w == 0 || h == 0)
            return;

        UInt running = 0;
        // build until 1x1 (inclusive)
        n_levels_ = 0;
    // while (true)
    build_pyramid_loop:
        for (int i = 0; i < 15; i++)
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

template <class T>
class TextureBRAM : public TextureBase<TextureBRAM<T>, T>
{
public:
    static constexpr int max_x = 80;
    static constexpr int max_y = 60;

    // Default-construct an empty texture. Safe to assign later.
    TextureBRAM() = default;

    TextureBRAM(UInt w, UInt h, T nodata, T *base)
        : nodata_(nodata), storage_(int(w * h * (1.0 + 1.0 / 4.0 + 1.0 / 8.0)), base)
    {
        build_pyramid_(w, h);
    }

    TextureBRAM(UInt w, UInt h, T nodata)
        : nodata_(nodata), storage_(int(w * h * (1.0 + 1.0 / 4.0 + 1.0 / 8.0)))
    {
        build_pyramid_(w, h);
    }

    // Rule of 5
    TextureBRAM(const TextureBRAM &) = default;
    TextureBRAM &operator=(const TextureBRAM &) = default;
    TextureBRAM(TextureBRAM &&) noexcept = default;
    TextureBRAM &operator=(TextureBRAM &&) noexcept = default;
    ~TextureBRAM() = default;

    // Introspection
    UInt width(UInt lvl) const { return levels_[lvl].w; }
    UInt height(UInt lvl) const { return levels_[lvl].h; }
    UInt levels() const { return n_levels_; }
    UInt size(UInt lvl) const { return width(lvl) * height(lvl); }
    UInt type_size() const { return sizeof(T); };
    T nodata() const { return nodata_; }

    // Fill a level with a constant
    void fill(UInt lvl, const T &v)
    {
    texturehls_fill_loop:
        for (int i = 0; i < size(lvl); i++)
        {
            storage_[levels_[lvl].offset + i] = v;
        }
    }

    // Read/Write a single texel (bounds-checked in debug)
    T texel_(UInt y, UInt x, UInt lvl) const
    {
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif
        return storage_[levels_[lvl].offset + y * levels_[lvl].w + x];
    }

    void set_texel_(const T &v, UInt y, UInt x, UInt lvl)
    {
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif
        storage_[levels_[lvl].offset + y * levels_[lvl].w + x] = v;
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

    BufferBRAM<T, int(max_x *max_y * (1.0 + 1.0 / 4.0 + 1.0 / 8.0))> storage_;

    T nodata_;

    void build_pyramid_(UInt w, UInt h)
    {
        if (w == 0 || h == 0)
            return;

        UInt running = 0;
        // build until 1x1 (inclusive)
        n_levels_ = 0;
    // while (true)
    build_pyramid_loop:
        for (int i = 0; i < 15; i++)
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
