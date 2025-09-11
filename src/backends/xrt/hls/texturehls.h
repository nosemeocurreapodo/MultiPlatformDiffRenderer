#pragma once

#include "backends/base/texturebase.h"
#include "backends/xrt/hls/bufferhls.h"

template <class T>
class TextureHLS : public TextureBase<TextureHLS<T>, T>
{
public:
    // using value_type = T;
    // using size_type = std::size_t;

    TextureHLS(std::size_t w, std::size_t h, T nodata, const T *base)
        : nodata_(nodata), storage_(w * h, const_cast<T *>(base))
    {
    }

    // Rule of 5
    TextureHLS(const TextureHLS &) = default;
    TextureHLS &operator=(const TextureHLS &) = default;
    TextureHLS(TextureHLS &&) noexcept = default;
    TextureHLS &operator=(TextureHLS &&) noexcept = default;
    ~TextureHLS() = default;

    // Introspection
    std::size_t width(std::size_t lvl) const { return levels_[lvl].w; }
    std::size_t height(std::size_t lvl) const { return levels_[lvl].h; }
    std::size_t levels() const { return levels_.size(); }
    std::size_t size(int lvl) const { return width(lvl) * height(lvl); }
    std::size_t type_size() const { return sizeof(T); };
    T nodata() const { return nodata_; }

    // Fill a level with a constant
    void fill(std::size_t lvl, const T &v)
    {
        auto m = MapWrite(lvl);
        std::fill(m.data(), m.data() + m.size(), v);
    }

    // Read/Write a single texel (bounds-checked in debug)
    T texel_(std::size_t y, std::size_t x, std::size_t lvl) const
    {
        assert(x < width(lvl) && y < height(lvl));

        const auto &L = levels_[lvl];
        return storage_[L.offset + y * L.w + x];
    }

    void set_texel_(const T &v, std::size_t y, std::size_t x, std::size_t lvl)
    {
        assert(x < width(lvl) && y < height(lvl));

        const auto &L = levels_[lvl];
        storage_[L.offset + y * L.w + x] = v;
    }

protected:
    template <class Mesh, class Texture>
    friend class DepthRendererBase;
    template <class Mesh, class TextureIn, class TextureOut>
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
        std::size_t offset; // element offset in storage_
        std::size_t size;   // elements at this level (w*h*channels)
        int w, h;
        // optional: std::size_t pitch; // elements per row if you pad rows
    };

    std::vector<Level> levels_;

    BufferHLS<T> storage_;

    T nodata_{};
};
