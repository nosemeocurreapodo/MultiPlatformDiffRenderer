#pragma once

#include "backends/xrt/hls/bufferhls.h"
#include "core/types.h"

template <class T>
class TextureRAM
{
public:
    TextureRAM() = default;

    TextureRAM(unsigned int w, unsigned int h, T nodata, T *base)
        : nodata_(nodata)
    {
        build_pyramid_(w, h);
        storage_ = BufferRAM<T>(total_size_, base);
    }

    // Rule of 5
    // TextureRAM(const TextureRAM &) = default;
    // TextureRAM &operator=(const TextureRAM &) = default;
    // TextureRAM(TextureRAM &&) noexcept = default;
    // TextureRAM &operator=(TextureRAM &&) noexcept = default;
    ~TextureRAM() = default;

    // Introspection
    unsigned int width(unsigned int lvl) const { return levels_[lvl].w; }
    unsigned int height(unsigned int lvl) const { return levels_[lvl].h; }
    unsigned int levels() const { return n_levels_; }
    // UInt size() const { return total_size_; }
    unsigned int type_size() const { return sizeof(T); };
    T nodata() const { return nodata_; }

    // Fill a level with a constant
    void fill(unsigned int lvl, const T &v)
    {
    texturehls_fill_loop:
        for (int i = 0; i < width(lvl) * height(lvl); i++)
        {
#pragma HLS loop_tripcount min = 307200 max = 307200 avg = 307200

            storage_[levels_[lvl].offset + i] = v;
        }
    }

    // Read/Write a single texel (bounds-checked in debug)
    T texel_(unsigned int y, unsigned int x, unsigned int lvl) const
    {
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif
        return storage_[levels_[lvl].offset + y * levels_[lvl].w + x];
    }

    void set_texel_(const T &v, unsigned int y, unsigned int x, unsigned int lvl)
    {
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif
        storage_[levels_[lvl].offset + y * levels_[lvl].w + x] = v;
    }

protected:
    // template <class Mesh, template <class> class Texture>
    // friend class DepthRendererBase;
    // template <class Mesh, template <class> class Texture>
    // friend class ImageRendererBase;
    //  friend class DepthRendererCPU;
    //  friend class ImageRendererCPU;
    //  friend class ResidualRendererCPU;
    //  friend class L2RendererCPU;
    //  friend class DIDxyRendererCPU;
    //  friend class JPoseRendererCPU;
    //  friend class JMapRendererCPU;

    struct Level
    {
        unsigned int offset; // element offset in storage_
        // UInt size;   // elements at this level (w*h*channels)
        unsigned int w, h;
        // optional: std::size_t pitch; // elements per row if you pad rows
    };

    void build_pyramid_(unsigned int w, unsigned int h)
    {
        if (w == 0 || h == 0)
            return;

        total_size_ = 0;
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
            // L.size = w * h;
            L.offset = total_size_;

            levels_[n_levels_] = L;
            total_size_ += w * h;
            n_levels_++;

            if (w == 1 && h == 1)
                break;

            w = max<unsigned int>(1, w >> 1);
            h = max<unsigned int>(1, h >> 1);
        }
    }

    unsigned int total_size_;
    Level levels_[15];
    unsigned int n_levels_;
    BufferRAM<T> storage_;
    T nodata_;
};

template <class T>
class TextureRAMCached
{
public:
    static constexpr int max_x = 32;
    static constexpr int max_y = 32;

    TextureRAMCached() = default;

    TextureRAMCached(unsigned int w, unsigned int h, T nodata, T *base)
        : nodata_(nodata)
    {
        build_pyramid_(w, h);
        ram_ = base;

        // ensure cache is invalid
        cache_x_ = w;
        cache_y_ = h;
        cache_lvl_ = 0;
        need_sync_write_ = false;
    }

    // Rule of 5
    // TextureRAM(const TextureRAM &) = default;
    // TextureRAM &operator=(const TextureRAM &) = default;
    // TextureRAM(TextureRAM &&) noexcept = default;
    // TextureRAM &operator=(TextureRAM &&) noexcept = default;
    ~TextureRAMCached()
    {
        if (need_sync_write_)
            cache_write_();
    }

    // Introspection
    unsigned int width(unsigned int lvl) const { return levels_[lvl].w; }
    unsigned int height(unsigned int lvl) const { return levels_[lvl].h; }
    unsigned int levels() const { return n_levels_; }
    // UInt size() const { return total_size_; }
    unsigned int type_size() const { return sizeof(T); };
    T nodata() const { return nodata_; }

    // Fill a level with a constant
    void fill(unsigned int lvl, const T &v)
    {
    texturehls_fill_loop:
        for (int i = 0; i < width(lvl) * height(lvl); i++)
        {
#pragma HLS loop_tripcount min = 307200 max = 307200 avg = 307200

            ram_[levels_[lvl].offset + i] = v;
        }
    }

    // Read/Write a single texel (bounds-checked in debug)
    T texel_(unsigned int y, unsigned int x, unsigned int lvl)
    {
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif

        sync_cache_(x, y, lvl);

        int lx = int(x) - cache_x_;
        int ly = int(y) - cache_y_;
        return cache_[lx + ly * max_x];
    }

    void set_texel_(const T &v, unsigned int y, unsigned int x, unsigned int lvl)
    {
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif

        sync_cache_(x, y, lvl);
        need_sync_write_ = true;

        int lx = int(x) - cache_x_;
        int ly = int(y) - cache_y_;
        cache_[lx + ly * max_x] = v;

        // ram_[levels_[lvl].offset + y * levels_[lvl].w + x] = v;
    }

protected:
    // template <class Mesh, template <class> class Texture>
    // friend class DepthRendererBase;
    // template <class Mesh, template <class> class Texture>
    // friend class ImageRendererBase;
    //  friend class DepthRendererCPU;
    //  friend class ImageRendererCPU;
    //  friend class ResidualRendererCPU;
    //  friend class L2RendererCPU;
    //  friend class DIDxyRendererCPU;
    //  friend class JPoseRendererCPU;
    //  friend class JMapRendererCPU;

    struct Level
    {
        unsigned int offset; // element offset in storage_
        // UInt size;   // elements at this level (w*h*channels)
        unsigned int w, h;
        // optional: std::size_t pitch; // elements per row if you pad rows
    };

    void build_pyramid_(unsigned int w, unsigned int h)
    {
        if (w == 0 || h == 0)
            return;

        total_size_ = 0;
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
            // L.size = w * h;
            L.offset = total_size_;

            levels_[n_levels_] = L;
            total_size_ += w * h;
            n_levels_++;

            if (w == 1 && h == 1)
                break;

            w = max<unsigned int>(1, w >> 1);
            h = max<unsigned int>(1, h >> 1);
        }
    }

    void sync_cache_(unsigned int x, unsigned int y, unsigned int lvl)
    {
        if (cache_miss_(x, y, lvl))
        {
            if (need_sync_write_)
                cache_write_();
            need_sync_write_ = false;
            cache_x_ = int(x) - max_x / 2;
            cache_y_ = int(y) - max_y / 2;
            cache_lvl_ = lvl;
            cache_read_();
        }
    }

    bool cache_miss_(unsigned int x, unsigned int y, unsigned int lvl)
    {
        if (x < cache_x_ || x >= cache_x_ + max_x ||
            y < cache_y_ || y >= cache_y_ + max_y || lvl != cache_lvl_)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    void cache_read_()
    {
        int w = width(cache_lvl_);
        int h = height(cache_lvl_);
        int offset = levels_[cache_lvl_].offset;

    texturehls_cache_read_y_loop:
        for (int j = 0; j < max_y; j++)
        {
#pragma HLS loop_tripcount min = max_y max = max_y avg = max_y
            // #pragma HLS PIPELINE II = 1

            int addr_y = cache_y_ + j;
            int ram_base = offset + addr_y * w + cache_x_;
            int bram_base = j * max_x;

            if (addr_y >= h || addr_y < 0)
            {
                continue;
            }

        texturehls_cache_read_x_loop:
            for (int i = 0; i < max_x; i++)
            {
#pragma HLS loop_tripcount min = max_x max = max_x avg = max_x
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II = 1

                // if (addr_x >= w || addr_x < 0)
                // {
                //    continue;
                //}

                cache_[bram_base + i] = ram_[ram_base + i];
            }
        }
    }

    void cache_write_()
    {
        int w = width(cache_lvl_);
        int h = height(cache_lvl_);
        int offset = levels_[cache_lvl_].offset;

    texturehls_cache_write_y_loop:
        for (int j = 0; j < max_y; j++)
        {
#pragma HLS loop_tripcount min = max_y max = max_y avg = max_y
            // #pragma HLS PIPELINE II = 1

            int addr_y = cache_y_ + j;
            int ram_base = offset + addr_y * w + cache_x_;
            int bram_base = j * max_x;

            if (addr_y >= h || addr_y < 0)
            {
                continue;
            }

        texturehls_cache_write_x_loop:
            for (int i = 0; i < max_x; i++)
            {
#pragma HLS loop_tripcount min = max_x max = max_x avg = max_x
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II = 1

                // if (addr_x >= w || addr_x < 0)
                //{
                //     continue;
                // }

                ram_[ram_base + i] = cache_[bram_base + i];
            }
        }
    }

    unsigned int total_size_;
    Level levels_[15];
    unsigned int n_levels_;
    T nodata_;

    int cache_x_;
    int cache_y_;
    int cache_lvl_;
    bool need_sync_write_;

    T *ram_;
    T cache_[int(max_x * max_y)];
};

template <class T>
class TextureBRAM
{
public:
    static constexpr int max_x = 80;
    static constexpr int max_y = 60;

    // Default-construct an empty texture. Safe to assign later.
    TextureBRAM() = default;

    TextureBRAM(unsigned int w, unsigned int h, T nodata)
        : nodata_(nodata)
    {
        build_pyramid_(w, h);
        storage_.size_ = total_size_;
    }

    TextureBRAM(unsigned int w, unsigned int h, T nodata, T *base)
        : TextureBRAM(w, h, nodata)
    {
        for (int i = 0; i < w * h; i++)
        {
#pragma HLS loop_tripcount min = 307200 max = 307200 avg = 307200

            storage_[i] = base[i];
        }
    }

    // Rule of 5
    // TextureBRAM(const TextureBRAM &) = default;
    // TextureBRAM &operator=(const TextureBRAM &) = default;
    // TextureBRAM(TextureBRAM &&) noexcept = default;
    // TextureBRAM &operator=(TextureBRAM &&) noexcept = default;
    ~TextureBRAM() = default;

    // Introspection
    unsigned int width(unsigned int lvl) const { return levels_[lvl].w; }
    unsigned int height(unsigned int lvl) const { return levels_[lvl].h; }
    unsigned int levels() const { return n_levels_; }
    unsigned int size() const { return total_size_; }
    unsigned int type_size() const { return sizeof(T); };
    T nodata() const { return nodata_; }

    // Fill a level with a constant
    void fill(unsigned int lvl, const T &v)
    {
    texturehls_fill_loop:
        for (int i = 0; i < width(lvl) * height(lvl); i++)
        {
#pragma HLS loop_tripcount min = 307200 max = 307200 avg = 307200

            storage_[levels_[lvl].offset + i] = v;
        }
    }

    // Read/Write a single texel (bounds-checked in debug)
    T texel_(unsigned int y, unsigned int x, unsigned int lvl) const
    {
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif
        return storage_[levels_[lvl].offset + y * levels_[lvl].w + x];
    }

    void set_texel_(const T &v, unsigned int y, unsigned int x, unsigned int lvl)
    {
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif
        storage_[levels_[lvl].offset + y * levels_[lvl].w + x] = v;
    }

protected:
    // template <class Mesh, template <class> class Texture>
    // friend class DepthRendererBase;
    // template <class Mesh, template <class> class Texture>
    // friend class ImageRendererBase;
    //  friend class DepthRendererCPU;
    //  friend class ImageRendererCPU;
    //  friend class ResidualRendererCPU;
    //  friend class L2RendererCPU;
    //  friend class DIDxyRendererCPU;
    //  friend class JPoseRendererCPU;
    //  friend class JMapRendererCPU;

    struct Level
    {
        unsigned int offset; // element offset in storage_
        // UInt size;   // elements at this level (w*h*channels)
        unsigned int w, h;
        // optional: std::size_t pitch; // elements per row if you pad rows
    };

    void build_pyramid_(unsigned int w, unsigned int h)
    {
        if (w == 0 || h == 0)
            return;

        total_size_ = 0;
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
            // L.size = w * h;
            L.offset = total_size_;

            levels_[n_levels_] = L;
            total_size_ += w * h;
            n_levels_++;

            if (w == 1 && h == 1)
                break;

            w = max<unsigned int>(1, w >> 1);
            h = max<unsigned int>(1, h >> 1);
        }
    }

    unsigned int total_size_;
    Level levels_[15];
    unsigned int n_levels_;
    BufferBRAM<T, int(max_x *max_y * (1.0 + 1.0 / 4.0 + 1.0 / 8.0))> storage_;
    T nodata_;
};
