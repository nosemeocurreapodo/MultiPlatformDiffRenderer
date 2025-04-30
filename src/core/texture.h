#pragma once

template <typename Type>
class Texture
{
public:
    virtual void FromCPU(Type *data) = 0;
    virtual void ToCPU(Type *data) const = 0;
    unsigned int width() const
    {
        return width_;
    }
    unsigned int height() const
    {
        return height_;
    }
    unsigned int channels() const
    {
        return channels_;
    }
    unsigned int size() const
    {
        return width_ * height_ * channels_;
    }

    Type nodata() const
    {
        return nodata_;
    }

private:
    unsigned int width_;
    unsigned int height_;
    unsigned int channels_;
    Type nodata_;
};
