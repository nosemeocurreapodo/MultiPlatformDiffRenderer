#pragma once

template <typename Type>
class TextureXRT
{

public:

    /*
    TextureCPU() : nodata_(0), width_(0), height_(0), channels_(0)
    {
        // data_ = nullptr;
    }
    */

    TextureCPU(int width, int height, int channels, Type nodata_value)
    {
        nodata_ = nodata_value;
        width_ = width;
        height_ = height;
        channels_ = channels;

        data_ = std::make_unique<Type[]>(channels * width * height);
        std::fill_n(data_, width * height, nodata_value);
    }

    TextureCPU(int width, int height, int channels, Type nodata_value, Type *data)
    {
        nodata_ = nodata_value;
        width_ = width;
        height_ = height;
        channels_ = channels;

        data_ = std::make_unique<Type[]>(channels * width * height);
        std::copy(data, data + channels_ * width_ * height_, data_.get());
    }

    TextureCPU(const TextureCPU &other)
    {
        nodata_ = other.nodata_;
        width_ = other.width_;
        height_ = other.height_;
        channels_ = other.channels_;
        data_ = std::make_unique<Type[]>(channels_ * width_ * height_);

        std::copy(other.data_.get(), other.data_.get() + channels_ * width_ * height_, data_.get());
    }

    TextureCPU &operator=(const TextureCPU &other)
    {
        if (this != &other)
        {
            nodata_ = other.nodata_;
            width_ = other.width_;
            height_ = other.height_;
            channels_ = other.channels_;

            data_ = std::make_unique<Type[]>(channels_ * width_ * height_);
            std::copy(other.data_.get(), other.data_.get() + channels_ * width_ * height_, data_.get());
        }
        return *this;
    }

    void FromCPU(Type *data)
    {
        std::copy(data, data + channels_ * width_ * height_, data_.get());
    }

    void ToCPU(Type *data)
    {
        std::copy(data_.get(), data_.get() + channels_ * width_ * height_, data);
    }

    /*
    ~data()
    {
        delete[] m_data;
        // m_data = nullptr;
    }
    */

    Type nodata_;
    int width_;
    int height_;
    int channels_;

private:
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
        assert(y >= 0 && x >= 0 && y < height_ && x < width_);

        int address = x + y * width_;
        data_[address] = value;
    }

    Type GetTexel(int y, int x) const
    {
        assert(y >= 0 && x >= 0 && y < height_ && x < width_);

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

    std::unique_ptr<Type[]> data_;
};
