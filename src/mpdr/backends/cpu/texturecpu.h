#pragma once
// #include <algorithm>
// #include <cassert>
// #include <cstddef>
// #include <type_traits>
// #include <utility>
// #include <vector>
// #include <cmath>
#include <cstring>
#include "mpdr/backends/base/texturebase.h"
#include "mpdr/backends/cpu/buffercpu.h"
#include "mpdr/common/boundingbox.h"
#include "mpdr/common/typeindex_common.h"
#include "mpdr/common/cv_converters.h"

// --- Minimal mapped view pieces (works with CPU/GL buffers too) ---
struct TextureCPUNoopReleaser
{
    void operator()() const noexcept {}
};

template <typename T>
using TextureViewReadCPU = TextureViewBase<T, TextureCPUNoopReleaser>;

template <typename T>
using TextureViewWriteCPU = TextureViewBase<T, TextureCPUNoopReleaser>;

template <class T>
class TextureCPU
{
public:
    // using value_type = T;
    // using size_type = UInt;

    // Default-construct an empty texture. Safe to assign later.
    // TextureCPU() = default;

    // Create empty pyramid filled with nodata
    TextureCPU(unsigned int w, unsigned int h, T nodata)
        : nodata_(nodata)
    {
        build_pyramid_(w, h);
        data_ = std::make_unique<T[]>(total_size_);
        // storage_ = BufferCPU<T>(total_size_);
        //  Fill base and all levels with nodata
        //  for (UInt lvl = 0; lvl < levels(); ++lvl)
        //     fill(lvl, nodata);
    }

    // Create and upload base level
    TextureCPU(unsigned int w, unsigned int h, T nodata, const T *base)
        : TextureCPU(w, h, nodata)
    {
        if (w > 0 && h > 0)
        {
            auto m = MapWrite(0);
            // std::copy_n(base, w * h, m.data());
            std::memcpy(m.data(), base, w * h * sizeof(T));
        }
    }

    // bounding box is between 0-1
    TextureCPU(const TextureCPU &tex, const BoundingBox<float> &bb)
        : TextureCPU(int(ceil(bb.width_ * tex.width(0))),
                     int(ceil(bb.height_ * tex.height(0))),
                     tex.nodata())
    {
        for (int lvl = 0; lvl < levels(); ++lvl)
        {
            auto r = tex.MapRead(lvl);
            auto m = MapWrite(lvl);

            BoundingBox<int> level_bb(int(floor(bb.min_x_ * tex.width(lvl))),
                                      int(ceil(bb.max_x_ * tex.width(lvl))),
                                      int(floor(bb.min_y_ * tex.height(lvl))),
                                      int(ceil(bb.max_y_ * tex.height(lvl))));

            for (int y = 0; y < height(lvl); y++)
            {
                for (int x = 0; x < width(lvl); x++)
                {
                    int src_y = y + level_bb.min_y_;
                    int src_x = x + level_bb.min_x_;

                    T val = r[src_y * tex.width(lvl) + src_x];
                    m[y * width(lvl) + x] = val;
                }
            }
        }
    }

    /*
    TextureCPU(const TextureCPU &other) : TextureCPU(other.width(0), other.height(0), other.nodata())
    {
        std::copy_n(other.data_.get(), other.total_size_, data_.get());
    }

    TextureCPU &operator=(const TextureCPU &other)
    {
        if (this != &other)
        {
            TextureCPU tmp(other);
            swap(tmp);
        }
        return *this;
    }
    */

    // Rule of 5
    TextureCPU(const TextureCPU &) = delete;
    TextureCPU &operator=(const TextureCPU &) = delete;
    TextureCPU(TextureCPU &&) noexcept = default;
    TextureCPU &operator=(TextureCPU &&) noexcept = default;
    ~TextureCPU() = default;

    // Introspection
    unsigned int width(unsigned int lvl) const { return levels_[lvl].w; }
    unsigned int height(unsigned int lvl) const { return levels_[lvl].h; }
    unsigned int levels() const { return levels_.size(); }
    Level level(int lvl) const { return levels_[lvl]; }
    // UInt size() const { return total_size_; }
    unsigned int type_size() const { return sizeof(T); };
    // std::type_index get_type_index() const { return GetTypeIndex<T>(); };
    int getOpenCVType() const
    {
        return GetOpenCVFormat<T>();
    }

    T nodata() const { return nodata_; }

    // Fill a level with a constant
    void fill(unsigned int lvl, const T &v)
    {
        auto m = MapWrite(lvl);
        std::fill(m.data(), m.data() + m.size(), v);
    }

    void generate_mipmaps(unsigned int base_lvl)
    {
        // build lower levels
        for (unsigned int lvl = base_lvl + 1; lvl < levels(); ++lvl)
        {
            // generate_mipmap_(lvl);
            generate_mipmap<T, TextureCPU>(*this, lvl);
        }
    }

    [[nodiscard]] TextureViewReadCPU<T> MapRead(unsigned int lvl) const
    {
        const auto &L = levels_[lvl];
        return TextureViewReadCPU<T>(data_.get() + L.offset, L.w, L.h, nodata_, TextureCPUNoopReleaser());
    }

    [[nodiscard]] TextureViewWriteCPU<T> MapWrite(unsigned int lvl)
    {
        const auto &L = levels_[lvl];
        return TextureViewWriteCPU<T>(data_.get() + L.offset, L.w, L.h, nodata_, TextureCPUNoopReleaser());
    }

    // forbid mapping temporaries (view would dangle)
    TextureViewReadCPU<const T> MapRead() const && = delete;
    TextureViewWriteCPU<T> MapWrite() && = delete;

protected:
    // Read/Write a single texel (bounds-checked in debug)
    /*
    T texel_(unsigned int y, unsigned int x, unsigned int lvl) const
    {
        assert(x < width(lvl) && y < height(lvl));

        const auto &L = levels_[lvl];
        return storage_.data()[L.offset + y * L.w + x];
    }

    void set_texel_(const T &v, unsigned int y, unsigned int x, unsigned int lvl)
    {
        assert(x < width(lvl) && y < height(lvl));

        const auto &L = levels_[lvl];
        storage_.data()[L.offset + y * L.w + x] = v;
    }
    */

    // void set_texel_(const T &v, unsigned int address, unsigned int lvl)
    // {
    // assert(x < width(lvl) && y < height(lvl));

    //   const auto &L = levels_[lvl];
    //  storage_.data()[L.offset + address] = v;
    // }

    void build_pyramid_(unsigned int w, unsigned int h)
    {
        levels_.clear();
        if (w == 0 || h == 0)
            return;

        total_size_ = 0;
        // build until 1x1 (inclusive)
        while (true)
        {
            // levels_.push_back(Level{w, h, BufferCPU<T>(w * h)});
            Level L;
            L.w = w;
            L.h = h;
            L.offset = total_size_;

            levels_.push_back(L);
            total_size_ += w * h;

            if (w == 1 && h == 1)
                break;

            w = std::max<unsigned int>(1, w >> 1);
            h = std::max<unsigned int>(1, h >> 1);
        }
    }

    void swap(TextureCPU &o) noexcept
    {
        std::swap(data_, o.data_);
        std::swap(total_size_, o.total_size_);
        std::swap(levels_, o.levels_);
        std::swap(nodata_, o.nodata_);
    }

    unsigned int total_size_ = 0;
    std::vector<Level> levels_;
    // BufferCPU<T> storage_;
    std::unique_ptr<T[]> data_;
    T nodata_{};
};
