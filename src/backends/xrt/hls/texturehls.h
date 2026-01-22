#pragma once

#include "backends/xrt/hls/bufferhls.h"
#include "backends/base/mappedviewbase.h"
#include "core/boundingbox.h"
#include <ap_int.h>

struct TextureHLSNoopReleaser
{
    void operator()() const noexcept {}
};

template <typename T>
using TextureViewReadHLS = TextureViewBase<T, TextureHLSNoopReleaser>;

template <typename T>
using TextureViewWriteHLS = TextureViewBase<T, TextureHLSNoopReleaser>;

template <class T>
class TextureRAM
{
public:
    TextureRAM() = default;

    TextureRAM(unsigned int w, unsigned int h, T nodata, T *base)
        : nodata_(nodata)
    {
        build_pyramid_(w, h);
        storage_ = base;
    }

    template <typename BoundingBoxType>
    TextureRAM(const TextureRAM &tex, const BoundingBoxType &bb)
        : TextureRAM(tex.width(0),
                     tex.height(0),
                     tex.nodata(),
                     tex.storage_)
    {
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
    Level level(int lvl) const { return levels_[lvl]; }
    unsigned int size() const { return total_size_; }
    unsigned int type_size() const { return sizeof(T); };
    T nodata() const { return nodata_; }

    // Fill a level with a constant
    void fill(unsigned int lvl, const T &v)
    {
        int base_address = levels_[lvl].offset;

    texturehls_fill_loop:
        for (int i = 0; i < width(lvl) * height(lvl); i++)
        {
#pragma HLS loop_tripcount min = 307200 max = 307200 avg = 307200

            storage_[base_address + i] = v;
        }
    }

    // Read/Write a single texel (bounds-checked in debug)
    T texel_(unsigned int y, unsigned int x, unsigned int lvl) const
    {
#pragma HLS inline
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif
        return storage_[levels_[lvl].offset + y * levels_[lvl].w + x];
    }

    void set_texel_(const T &v, unsigned int y, unsigned int x, unsigned int lvl)
    {
#pragma HLS inline
        // #ifndef __SYNTHESIS__
        //         assert(x < width(lvl) && y < height(lvl));
        // #endif
        storage_[levels_[lvl].offset + y * levels_[lvl].w + x] = v;
    }

    // T *data(unsigned int lvl) noexcept { return &storage_[levels_[lvl].offset]; }
    // const T *data(unsigned int lvl) const noexcept { return &storage_[levels_[lvl].offset]; }

private:

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
    T *storage_;
    T nodata_;
};

template <class T>
class TextureBRAM
{
public:
    static constexpr int W = 32;
    static constexpr int H = 32;

    TextureBRAM() = default;

    TextureBRAM(unsigned int w, unsigned int h, T nodata)
        : nodata_(nodata)
    {
        build_pyramid_(w, h);
    }

    TextureBRAM(unsigned int w, unsigned int h, T nodata, T *base)
        : nodata_(nodata)
    {
        build_pyramid_(w, h);
    texturebram_copy_loop:
        for (int i = 0; i < total_size_; i++)
        {
#pragma HLS loop_tripcount min = 422400 max = 422400 avg = 422400

            storage_[i] = base[i];
        }
    }

    // Rule of 5
    // TextureRAM(const TextureRAM &) = default;
    // TextureRAM &operator=(const TextureRAM &) = default;
    // TextureRAM(TextureRAM &&) noexcept = default;
    // TextureRAM &operator=(TextureRAM &&) noexcept = default;
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

private:
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
    T storage_[int(W * H * (1.0f + 1.0f / 4.0f + 1.0f / 8.0f))];
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

private:
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
class TextureRAMCached2
{
public:
    using ram_type = ap_uint<32>;

    static constexpr int cache_w = 128;
    static constexpr int cache_h = 128;
    static constexpr int cache_banks = 2;
    static constexpr int type_bits = sizeof(T) * 8;
    static constexpr int ram_lanes = sizeof(ram_type) / sizeof(T);

    TextureRAMCached2() = default;

    TextureRAMCached2(unsigned int w, unsigned int h, T nodata, ram_type *base)
        : nodata_(nodata)
    {
        build_pyramid_(w, h);
        ram_ = base;

#pragma HLS BIND_STORAGE variable = cache_ type = ram_t2p impl = bram // uram
#pragma HLS array_partition variable = cache_ complete dim = 1
#pragma HLS array_partition variable = cache_ cyclic factor = 2 dim = 2
    }

    // Rule of 5
    // TextureRAM(const TextureRAM &) = default;
    // TextureRAM &operator=(const TextureRAM &) = default;
    // TextureRAM(TextureRAM &&) noexcept = default;
    // TextureRAM &operator=(TextureRAM &&) noexcept = default;
    ~TextureRAMCached2()
    {
        // if (need_sync_write_)
        //     cache_write_();
    }

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
        int base_address = levels_[lvl].offset / ram_lanes;
        int size = width(lvl) * height(lvl) / ram_lanes;
    fill_loop:
        for (int i = 0; i < size; i++)
        {
#pragma HLS loop_tripcount min = 307200 / ram_lanes max = 307200 / ram_lanes avg = 307200 / ram_lanes

            ram_type bits;
            for (int lane = 0; lane < ram_lanes; lane++)
            {
#pragma HLS UNROLL
                bits.range(type_bits * (lane + 1) - 1, type_bits * lane) = v;
            }

            ram_[base_address + i] = bits;
        }
    }

    void fill_cache(const T &v, int bank)
    {
#pragma HLS inline

    texturehls_fill_cache_loop:
        for (int i = 0; i < cache_bb_[bank].width_ * cache_bb_[bank].height_; i++)
        {
#pragma HLS loop_tripcount min = cache_h *cache_w / 4 max = cache_h *cache_w / 4 avg = cache_h * cache_w / 4

            cache_[bank][i] = v;
        }
    }

    // Read/Write a single texel (bounds-checked in debug)
    T texel_(unsigned int y, unsigned int x, unsigned int lvl)
    {
#pragma HLS inline

        int lx = int(x) - cache_bb_[cache_bank_read_].min_x_;
        int ly = int(y) - cache_bb_[cache_bank_read_].min_y_;

        // #ifndef __SYNTHESIS__
        //         assert(lx < cache_bb_.width_ && ly < cache_bb_.height_ && lvl == cache_lvl_);
        // #endif

        int bram_address = ly * cache_bb_[cache_bank_read_].width_ + lx;

        return cache_[cache_bank_read_][bram_address];
        // return ram_[levels_[lvl].offset + y * levels_[lvl].w + x];
    }

    void set_texel_(const T &v, unsigned int y, unsigned int x, unsigned int lvl)
    {
#pragma HLS inline

        int lx = int(x) - cache_bb_[cache_bank_write_].min_x_;
        int ly = int(y) - cache_bb_[cache_bank_write_].min_y_;

        // #ifndef __SYNTHESIS__
        //         assert(lx < cache_bb_.width_ && ly < cache_bb_.height_ && lvl == cache_lvl_);
        // #endif

        int bram_address = ly * cache_bb_[cache_bank_write_].width_ + lx;

        cache_[cache_bank_write_][bram_address] = v;
        // ram_[levels_[lvl].offset + y * levels_[lvl].w + x] = v;
    }

    void set_cache_bb_read_(const BoundingBox<int> &bb, int lvl, int bank)
    {
#pragma HLS inline

        cache_bb_[bank] = bb;
        cache_lvl_[bank] = lvl;
        cache_bank_read_ = bank;
    }

    void set_cache_bb_write_(const BoundingBox<int> &bb, int lvl, int bank)
    {
#pragma HLS inline

        cache_bb_[bank] = bb;
        cache_lvl_[bank] = lvl;
        cache_bank_write_ = bank;
    }

    void cache_read_(int bank)
    {
#pragma HLS INLINE

        int w = width(cache_lvl_[bank]);
        int h = height(cache_lvl_[bank]);
        int offset = levels_[cache_lvl_[bank]].offset;

        // Create restricted local aliases so HLS knows they don’t alias.
        // const T *__restrict src = ram_; // m_axi
        // T *__restrict dst = &cache_[0]; // BRAM

        // Assert away false loop-carried deps on the BRAM buffer.
        // #pragma HLS DEPENDENCE variable = cache_ inter false
        // #pragma HLS DEPENDENCE variable = ram_ inter false

        // #pragma HLS BIND_STORAGE variable = cache_ type = ram_1p impl = bram

    texturehls_cache_read_y_loop:
        for (int j = 0; j < cache_bb_[bank].height_; j++)
        {
#pragma HLS loop_tripcount min = cache_h / 2 max = cache_h / 2 avg = cache_h / 2
            // #pragma HLS PIPELINE II = 1

            int addr_y = cache_bb_[bank].min_y_ + j;
            int ram_base = (offset + addr_y * w + cache_bb_[bank].min_x_) / ram_lanes;
            int bram_base = j * cache_bb_[bank].width_;

            /*
        texturehls_cache_read_x_loop:
            for (int i = 0; i < cache_bb_.width_; i++)
            {
#pragma HLS loop_tripcount min = cache_w max = cache_w avg = cache_w
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II = 1

                // dst[bram_base + i] = src[ram_base + i];
                cache_[bram_base + i] = ram_[ram_base + i];
            }
            */

        texturehls_cache_read_x_loop:
            for (int i = 0; i < cache_bb_[bank].width_ / ram_lanes; i++)
            {
#pragma HLS loop_tripcount min = cache_w / (2 * ram_lanes) max = cache_w / (2 * ram_lanes) avg = cache_w / (2 * ram_lanes)
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II = 1

                ram_type data = ram_[ram_base + i];

                for (int lane = 0; lane < ram_lanes; lane++)
                {
#pragma HLS UNROLL
                    ap_uint<type_bits> bits = data.range(type_bits * (lane + 1) - 1, type_bits * lane);
                    T f = *reinterpret_cast<T *>(&bits);
                    cache_[bank][bram_base + i * ram_lanes + lane] = f;
                }
            }
        }
    }

    void cache_write_(int bank)
    {
#pragma HLS INLINE

        int w = width(cache_lvl_[bank]);
        int h = height(cache_lvl_[bank]);
        int offset = levels_[cache_lvl_[bank]].offset;

    texturehls_cache_write_y_loop:
        for (int j = 0; j < cache_bb_[bank].height_; j++)
        {
#pragma HLS loop_tripcount min = cache_h / 2 max = cache_h / 2 avg = cache_h / 2
            // #pragma HLS PIPELINE II = 1

            int addr_y = cache_bb_[bank].min_y_ + j;
            int ram_base = (offset + addr_y * w + cache_bb_[bank].min_x_) / ram_lanes;
            int bram_base = j * cache_bb_[bank].width_;

        texturehls_cache_write_x_loop:
            for (int i = 0; i < cache_bb_[bank].width_ / ram_lanes; i++)
            {
#pragma HLS loop_tripcount min = cache_w / (2 * ram_lanes) max = cache_w / (2 * ram_lanes) avg = cache_w / (2 * ram_lanes)
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II = 1

                ram_type ram_data;

                for (int lane = 0; lane < ram_lanes; lane++)
                {
#pragma HLS UNROLL
                    T bram_data = cache_[bank][bram_base + i * ram_lanes + lane];
                    ap_uint<type_bits> bram_bits = *reinterpret_cast<ap_uint<type_bits> *>(&bram_data);
                    ram_data.range(type_bits * (lane + 1) - 1, type_bits * lane) = bram_bits;
                }

                ram_[ram_base + i] = ram_data;
            }
        }
    }

    // private:
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
    T nodata_;

    ram_type *ram_;
    T cache_[cache_banks][cache_w * cache_h];
    BoundingBox<int> cache_bb_[cache_banks];
    int cache_lvl_[cache_banks];
    int cache_bank_read_;
    int cache_bank_write_;
};
