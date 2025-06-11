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
    : width_(width),
      height_(height),
      channels_(channels),
      nodata_(nodata_value),
      data_(data)
    {
        // assert(data != nullptr);
        // assert(width > 0 && height > 0 && channels > 0);
    }

    TextureFPGA(const TextureFPGA &other)
    : width_(other.width_),
      height_(other.height_),
      channels_(other.channels_),
      nodata_(other.nodata_),
      data_(other.data_)
    {
        // assert(data != nullptr);
        // assert(width > 0 && height > 0 && channels > 0);
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

    void SetTexel(Type value, fpga::Int y, fpga::Int x)
    {
        // assert(y >= 0 && x >= 0 && y < height_ && x < width_);

        fpga::Int address = x + y * width_;
        data_[address] = value;
    }

    Type GetTexel(fpga::Int y, fpga::Int x) const
    {
        // assert(y >= 0 && x >= 0 && y < height_ && x < width_);

        fpga::Int address = x + y * width_;
        return data_[address];
    }

    Type Get(fpga::Scalar norm_y, fpga::Scalar norm_x) const
    {
        fpga::Scalar wrapped_y = norm_y;
        fpga::Scalar wrapped_x = norm_x;
        if (wrapped_y < fpga::Scalar(0.0))
            // wrapped_y = std::fabs(wrapped_y);
            wrapped_y = -wrapped_y;
        if (wrapped_x < fpga::Scalar(0.0))
            // wrapped_x = std::fabs(wrapped_x);
            wrapped_x = -wrapped_x;
        if (wrapped_y > fpga::Scalar(1.0))
            wrapped_y = fpga::Scalar(1.0) - (wrapped_y - fpga::Scalar(1.0));
        if (wrapped_x > fpga::Scalar(1.0))
            wrapped_x = fpga::Scalar(1.0) - (wrapped_x - fpga::Scalar(1.0));
        fpga::Scalar x = wrapped_x * fpga::Scalar(width_ - 1);
        fpga::Scalar y = wrapped_y * fpga::Scalar(height_ - 1);
        return Bilinear(y, x);
    }

    Type Bilinear(fpga::Scalar y, fpga::Scalar x) const
    {
        // bilinear interpolation (-2 because the read the next pixel)
        // int _x = std::min(std::max(int(x), 0), texture[lvl].cols-2);
        // int _y = std::min(std::max(int(y), 0), texture[lvl].rows-2);
        if (y > fpga::Scalar(height_ - 2) || x > fpga::Scalar(width_ - 2))
            return GetTexel(fpga::Int(y), fpga::Int(x));

        fpga::Scalar _x = round(x);
        fpga::Scalar _y = round(y);
        fpga::Scalar dx = x - _x;
        fpga::Scalar dy = y - _y;

        fpga::Scalar weight_tl = (fpga::Scalar(1) - dx) * (fpga::Scalar(1) - dy);
        fpga::Scalar weight_tr = (dx) * (fpga::Scalar(1) - dy);
        fpga::Scalar weight_bl = (fpga::Scalar(1) - dx) * (dy);
        fpga::Scalar weight_br = (dx) * (dy);

        fpga::Int i_x = fpga::Int(_x);
        fpga::Int i_y = fpga::Int(_y);

        Type tl = GetTexel(i_y, i_x);
        Type tr = GetTexel(i_y, i_x + 1);
        Type bl = GetTexel(i_y + 1, i_x);
        Type br = GetTexel(i_y + 1, i_x + 1);

        if (tl == nodata_ || tr == nodata_ || bl == nodata_ || br == nodata_)
            return nodata_;

        Type pix = Type(fpga::Scalar(tl) * weight_tl +
                        fpga::Scalar(tr) * weight_tr +
                        fpga::Scalar(bl) * weight_bl +
                        fpga::Scalar(br) * weight_br);

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
    const Type nodata_;
    const fpga::UInt width_;
    const fpga::UInt height_;
    const fpga::UInt channels_;
};
