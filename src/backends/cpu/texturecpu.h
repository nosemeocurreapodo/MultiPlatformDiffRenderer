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
        : data_(width * height)
    {
        nodata_ = nodata_value;
        width_ = width;
        height_ = height;
        // data_.fill(nodata_value);
    }

    TextureCPU(int width, int height, Type nodata_value, Type *data)
        : data_(width * height, data)
    {
        nodata_ = nodata_value;
        width_ = width;
        height_ = height;
    }

    TextureCPU(const TextureCPU &other)
        : data_(other.data_)
    {
        nodata_ = other.nodata_;
        width_ = other.width_;
        height_ = other.height_;
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

    void FromCPU(const Type *data)
    {
        data_.FromCPU(data);
    }

    void ToCPU(Type *data) const
    {
        data_.ToCPU(data);
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

    unsigned int width() const
    {
        return width_;
    }
    unsigned int height() const
    {
        return height_;
    }
    unsigned int size() const
    {
        return width_ * height_;
    }

    Type nodata() const
    {
        return nodata_;
    }

protected:
    void SetTexel(Type value, int y, int x)
    {
        assert(y >= 0 && x >= 0 && y < height_ && x < width_);

        data_[x + y * width_] = value;
    }

    Type GetTexel(int y, int x) const
    {
        assert(y >= 0 && x >= 0 && y < height_ && x < width_);

        // int address = x + y * width_;
        return data_[x + y * width_];
    }

    Type Get(float norm_y, float norm_x) const
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
        float x = wrapped_x * (width_ - 1);
        float y = wrapped_y * (height_ - 1);
        return Bilinear(y, x);
    }

    void fill(const Type &value)
    {
        data_.fill(value);
    }

    TextureCPU<Type> GenerateMipmap()
    {
        TextureCPU<Type> mipmap(width_ / 2, height_ / 2, nodata_);

        for (int y = 0; y < height_ / 2; y++)
        {
            for (int x = 0; x < width_ / 2; x++)
            {
                Type pixel = Area(y * 2, x * 2);
                mipmap.SetTexel(pixel, y, x);
            }
        }
        return mipmap;
    }

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

    Type Bilinear(float y, float x) const
    {
        // bilinear interpolation (-2 because the read the next pixel)
        // int _x = std::min(std::max(int(x), 0), texture[lvl].cols-2);
        // int _y = std::min(std::max(int(y), 0), texture[lvl].rows-2);
        if (y > height_ - 2 || x > width_ - 2)
            return GetTexel(int(y), int(x));

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

        Type tl = GetTexel(_y, _x);
        Type tr = GetTexel(_y, _x + 1);
        Type bl = GetTexel(_y + 1, _x);
        Type br = GetTexel(_y + 1, _x + 1);

        if (tl == nodata_ || tr == nodata_ || bl == nodata_ || br == nodata_)
            return nodata_;

        Type pix = Type(tl * weight_tl +
                        tr * weight_tr +
                        bl * weight_bl +
                        br * weight_br);

        return pix;
    }

    Type Area(int y, int x) const
    {
        // bilinear interpolation (-2 because the read the next pixel)
        // int _x = std::min(std::max(int(x), 0), texture[lvl].cols-2);
        // int _y = std::min(std::max(int(y), 0), texture[lvl].rows-2);
        if (y > height_ - 2 || x > width_ - 2)
            return GetTexel(y, x);

        Type tl = GetTexel(y, x);
        Type tr = GetTexel(y, x + 1);
        Type bl = GetTexel(y + 1, x);
        Type br = GetTexel(y + 1, x + 1);

        if (tl == nodata_ || tr == nodata_ || bl == nodata_ || br == nodata_)
            return nodata_;

        Type pix = Type((tl + tr + bl + br) / 4.0f);

        return pix;
    }

    BufferCPU<Type> data_;

    unsigned int width_;
    unsigned int height_;
    Type nodata_;
};

template <typename Type>
class TextureMipMapCPU
{
public:
    TextureMipMapCPU()
    {
    }

    TextureMipMapCPU(int _width, int _height, Type _nodata_value)
    {
        int width = _width;
        int height = _height;
        nodata_ = _nodata_value;

        while (true)
        {
            TextureCPU<Type> lvlData(width, height, nodata_);
            data_.push_back(lvlData);

            width = int(width / 2);
            height = int(height / 2);

            if (width <= 1 || height <= 1)
                break;
        }
    }

    TextureMipMapCPU(const TextureCPU<Type> image)
    {
        TextureCPU<Type> imageLoD = image;
        nodata_ = image.nodata_;

        while (true)
        {
            data_.push_back(imageLoD);
            imageLoD = imageLoD.generateMipmap();

            if (imageLoD.width_ <= 1 || imageLoD.height_ <= 1)
                break;
        }
    }

    TextureMipMapCPU(const TextureMipMapCPU &other)
    {
        nodata_ = other.nodata_;
        for (size_t lvl = 0; lvl < other.data_.size(); lvl++)
        {
            data_.push_back(other.data_[lvl]);
        }
    }

    TextureMipMapCPU &operator=(const TextureMipMapCPU &other)
    {
        if (this != &other)
        {
            nodata_ = other.nodata_;
            data_.clear();

            for (size_t lvl = 0; lvl < other.data_.size(); lvl++)
            {
                data_.push_back(other.data_[lvl]);
            }
        }
        return *this;
    }

    /*
    void SetToNoData(int lvl)
    {
        data_[lvl].set(nodata_);
    }
    */

    void GenerateMipmaps(int baselvl = 0)
    {
        for (size_t lvl = baselvl + 1; lvl < data_.size(); lvl++)
        {
            TextureCPU<Type> d = data_[lvl - 1].generateMipmap();
            data_[lvl] = d;
        }
    }

    int GetLvls()
    {
        return data_.size();
    }

    Type nodata_;

private:
    std::vector<TextureCPU<Type>> data_;
};