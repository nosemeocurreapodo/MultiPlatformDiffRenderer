#pragma once

template <typename Type>
class TextureFPGA
{
    template <typename InTexType,
              typename VaryingType,
              typename OutTexType,
              typename Derived>
    friend class BaseRendererFPGA;
    friend class ImageRendererFPGA;

public:
    /*
    TextureCPU() : nodata_(0), width_(0), height_(0), channels_(0)
    {
        // data_ = nullptr;
    }
    */

    TextureFPGA(unsigned int width, unsigned int height, unsigned int channels, Type nodata_value, Type *data)
    {
        width_ = width;
        height_ = height;
        channels_ = channels;
        nodata_ = nodata_value;
        data_ = data;
    }

    TextureFPGA(const TextureFPGA &other)
    {
        width_ = other.width_;
        height_ = other.height_;
        channels_ = other.channels_;
        nodata_ = other.nodata_;
        data_ = other.data_;
    }

    TextureFPGA &operator=(const TextureFPGA &other)
    {
        if (this != &other)
        {
            width_ = other.width_;
            height_ = other.height_;
            channels_ = other.channels_;
            nodata_ = other.nodata_;
            data_ = other.data_;
        }
        return *this;
    }

protected:
    /*
    void set(const Type value)
    {
        std::fill_n(m_data, width * height, value);
    }
    */

    /*
    void setToNoData()
    {
        set(nodata);
    }
    */

    void SetTexel(Type value, int y, int x)
    {
        // assert(y >= 0 && x >= 0 && y < height_ && x < width_);

        int address = x + y * width_;
        data_[address] = value;
    }

    Type GetTexel(int y, int x) const
    {
        // assert(y >= 0 && x >= 0 && y < height_ && x < width_);

        int address = x + y * width_;
        return data_[address];
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

    Type Bilinear(float y, float x) const
    {
        // bilinear interpolation (-2 because the read the next pixel)
        // int _x = std::min(std::max(int(x), 0), texture[lvl].cols-2);
        // int _y = std::min(std::max(int(y), 0), texture[lvl].rows-2);
        if (y > height_ - 2 || x > width_ - 2)
            return GetTexel(int(y), int(x));

        int _x = int(x);
        int _y = int(y);
        float dx = x - _x;
        float dy = y - _y;

        float weight_tl = (1.0 - dx) * (1.0 - dy);
        float weight_tr = (dx) * (1.0 - dy);
        float weight_bl = (1.0 - dx) * (dy);
        float weight_br = (dx) * (dy);

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

    Type *data_;
    Type nodata_;
    int width_;
    int height_;
    int channels_;
};
