#pragma once

#include "backends/cpu/buffercpu.h"

template <typename Type>
class TextureCPU
{
    template <typename InTexType, typename VaryingType, typename OutTexType>
    friend class BaseRendererCPU;
    friend class ImageRendererCPU;
    friend class DIDxyRendererCPU;
    friend class JtraRendererCPU;
    friend class JrotRendererCPU;
    friend class JPoseRendererCPU;

public:
    TextureCPU() : nodata_(0), width_(0), height_(0)
    {
        // data_ = nullptr;
    }

    TextureCPU(int width, int height, Type nodata_value)
    {
        nodata_ = nodata_value;
        // width_ = width;
        // height_ = height;
        //  data_.fill(nodata_value);

        int lvl = 0;
        while (true)
        {
            int width_lvl = int(width / std::pow(2, lvl));
            int height_lvl = int(height / std::pow(2, lvl));

            if (width_lvl == 0 || height_lvl == 0)
                break;

            width_.push_back(width_lvl);
            height_.push_back(height_lvl);

            BufferCPU<Type> data_lvl(width_lvl * height_lvl);
            data_.push_back(data_lvl);
            lvl++;
        }
    }

    TextureCPU(int width, int height, Type nodata_value, Type *data)
    {
        nodata_ = nodata_value;

        BufferCPU<Type> d(width * height, data);
        data_.push_back(d);
        width_.push_back(width);
        height_.push_back(height);

        int lvl = 1;
        while (true)
        {
            int width_lvl = int(width / std::pow(2, lvl));
            int height_lvl = int(height / std::pow(2, lvl));

            if (width_lvl == 0 || height_lvl == 0)
                break;

            width_.push_back(width_lvl);
            height_.push_back(height_lvl);

            BufferCPU<Type> data_lvl = GenerateMipmap(lvl);
            data_.push_back(data_lvl);
            lvl++;
        }
    }

    TextureCPU(const TextureCPU &other)
    {
        nodata_ = other.nodata_;
        width_ = other.width_;
        height_ = other.height_;
        data_ = other.data_;
    }

    TextureCPU &operator=(const TextureCPU &other)
    {
        if (this != &other)
        {
            nodata_ = other.nodata_;
            width_ = other.width_;
            height_ = other.height_;
            data_ = other.data_;
        }
        return *this;
    }

    void FromCPU(int lvl, const Type *data)
    {
        data_[lvl].FromCPU(data);
    }

    void ToCPU(int lvl, Type *data) const
    {
        data_[lvl].ToCPU(data);
    }

    /*
    const Type *get() const
    {
        return data_.get();
    }

    Type *get()
    {
        return data_.get();
    }
    */

    unsigned int width(int lvl) const
    {
        return width_[lvl];
    }
    unsigned int height(int lvl) const
    {
        return height_[lvl];
    }
    unsigned int size(int lvl) const
    {
        return data_[lvl].size();
    }

    Type nodata() const
    {
        return nodata_;
    }

protected:
    void SetTexel(Type value, int y, int x, int lvl)
    {
        assert(y >= 0 && x >= 0 && y < height_[lvl] && x < width_[lvl] && lvl >= 0 && lvl < data_.size());

        data_[lvl][x + y * width_[lvl]] = value;
    }

    Type GetTexel(int y, int x, int lvl) const
    {
        assert(y >= 0 && x >= 0 && y < height_[lvl] && x < width_[lvl] && lvl >= 0 && lvl < data_.size());

        // int address = x + y * width_;
        return data_[lvl][x + y * width_[lvl]];
    }

    Type Get(float norm_y, float norm_x, int lvl) const
    {
        float wrapped_y = norm_y;
        float wrapped_x = norm_x;
        if (wrapped_y < 0.0)
            // wrapped_y = std::fabs(wrapped_y);
            wrapped_y = -wrapped_y;
        if (wrapped_x < 0.0)
            // wrapped_x = std::fabs(wrapped_x);
            wrapped_x = -wrapped_x;
        if (wrapped_y > 1.0)
            wrapped_y = 1.0 - (wrapped_y - 1.0);
        if (wrapped_x > 1.0)
            wrapped_x = 1.0 - (wrapped_x - 1.0);
        float x = wrapped_x * (width_[lvl] - 1);
        float y = wrapped_y * (height_[lvl] - 1);
        return Bilinear(y, x, lvl);
    }

    void fill(int lvl, const Type &value)
    {
        data_[lvl].fill(value);
    }

    BufferCPU<Type> GenerateMipmap(int lvl)
    {
        int width_lvl = width_[lvl];
        int height_lvl = height_[lvl];

        BufferCPU<Type> mipmap(width_lvl * height_lvl);

        for (int y = 0; y < height_lvl; y++)
        {
            for (int x = 0; x < width_lvl; x++)
            {
                Type pixel = Area(y * 2, x * 2, lvl - 1);
                mipmap[x + y * width_lvl] = pixel;
            }
        }
        return mipmap;
    }
    /*
    template <typename type2>
    TextureCPU<type2> Convert() const
    {
        TextureCPU<type2> result(width_, height_, type2(nodata_));
        for (int y = 0; y < height_; y++)
            for (int x = 0; x < width_; x++)
            {
                Type d = GetTexel(y, x);
                if (d == nodata_)
                    continue;
                type2 res = type2(d);
                result.SetTexel(res, y, x);
            }
        return result;
    }
    */

    Type Bilinear(float y, float x, int lvl) const
    {
        // bilinear interpolation (-2 because the read the next pixel)
        // int _x = std::min(std::max(int(x), 0), texture[lvl].cols-2);
        // int _y = std::min(std::max(int(y), 0), texture[lvl].rows-2);
        if (y > height_[lvl] - 2 ||
            x > width_[lvl] - 2)
            return GetTexel(int(y), int(x), lvl);

        float _x = floor(x);
        float _y = floor(y);
        float dx = x - _x;
        float dy = y - _y;

        float weight_tl = (1.0 - dx) * (1.0 - dy);
        float weight_tr = (dx) * (1.0 - dy);
        float weight_bl = (1.0 - dx) * (dy);
        float weight_br = (dx) * (dy);

        int i_x = int(_x);
        int i_y = int(_y);

        Type tl = GetTexel(_y, _x, lvl);
        Type tr = GetTexel(_y, _x + 1, lvl);
        Type bl = GetTexel(_y + 1, _x, lvl);
        Type br = GetTexel(_y + 1, _x + 1, lvl);

        if (tl == nodata_ || tr == nodata_ || bl == nodata_ || br == nodata_)
            return nodata_;

        Type pix = Type(tl * weight_tl +
                        tr * weight_tr +
                        bl * weight_bl +
                        br * weight_br);

        return pix;
    }

    Type Area(int y, int x, int lvl) const
    {
        // bilinear interpolation (-2 because the read the next pixel)
        // int _x = std::min(std::max(int(x), 0), texture[lvl].cols-2);
        // int _y = std::min(std::max(int(y), 0), texture[lvl].rows-2);
        if (y > height_[lvl] - 2 || x > width_[lvl] - 2)
            return GetTexel(y, x);

        Type tl = GetTexel(y, x, lvl);
        Type tr = GetTexel(y, x + 1, lvl);
        Type bl = GetTexel(y + 1, x, lvl);
        Type br = GetTexel(y + 1, x + 1, lvl);

        if (tl == nodata_ || tr == nodata_ || bl == nodata_ || br == nodata_)
            return nodata_;

        Type pix = Type((tl + tr + bl + br) / 4.0f);

        return pix;
    }

    std::vector<BufferCPU<Type>> data_;
    std::vector<unsigned int> width_;
    std::vector<unsigned int> height_;
    Type nodata_;
};
