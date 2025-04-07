#pragma once

#include <pangolin/pangolin.h>
#include "src/common/types.h"

template <typename Type>
class dataMulti
{
public:

    dataMulti(int _width, int _height, int _channels, Type _nodata_value)
    {
        nodata = _nodata_value;
        width = _width;
        height = _height;
        channels = _channels;

        // by default, data is in CPU
        currentDevice = devices["CPU"];

        // by default, cpu memory buffer is always present
        cpu_data = std::make_unique<Type>(Type[channels * width * height]);
        if (cpu_data == nullptr)
            throw std::bad_alloc("Failed creating data");

        set(_nodata_value);
    }

    data(const data &other)
    {
        nodata = other.nodata;
        width = other.width;
        height = other.height;
        channels = other.channels;
        currentDevice = other.currentDevice;
        cpu_data = std::make_unique<Type>(Type[channels * width * height]);
        
        if(currentDevice == devices["CPU"])
            std::memcpy(cpu_data, other.cpu_data, sizeof(Type) * channels * width * height);
    }

    data &operator=(const data &other)
    {
        if (this != &other)
        {
            delete cpu_data;

            nodata = other.nodata;
            width = other.width;
            height = other.height;
            channels = other.channels;
            cpu_data = new Type[channels * width * height];

            std::memcpy(cpu_data, other.cpu_data, sizeof(Type) * channels * width * height);
        }
        return *this;
    }

    /*
    ~data()
    {
        delete[] m_data;
        // m_data = nullptr;
    }
    */

    void toDevice(std:string device_name)
    {
        if(devices.count(device_name) == 0)
            throw std::runtime_error("Invalid device name");

        if(devices[device_name] == currentDevice)
            return;

        currentDevice = devices[device_name];

        if(currentDevice == devices["OpenGL"])
        {
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            GLint internalFormat = GL_R32F;
            GLenum openglImageType = GL_FLOAT;

            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.width, image.height, 0, GL_RED, openglImageType, image.get());
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    void setTexel(const Type value, int y, int x)
    {
        assert(y >= 0 && x >= 0 && y < height && x < width);

        int address = x + y * width;
        m_data[address] = value;
    }

    void set(const Type value)
    {
        std::fill_n(m_data, width * height, value);
    }

    void setToNoData()
    {
        set(nodata);
    }

    void set(Type *data)
    {
        std::memcpy(m_data, data, sizeof(Type) * width * height);
    }

    Type getTexel(int y, int x) const
    {
        assert(y >= 0 && x >= 0 && y < height && x < width);

        int address = x + y * width;
        return m_data[address];
    }

    Type get(float norm_y, float norm_x) const
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
        float x = wrapped_x * (width - 1);
        float y = wrapped_y * (height - 1);
        return bilinear(y, x);
    }

    Type *get()
    {
        return m_data;
    }

    float getPercentNoData()
    {
        int nodatacount = 0;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                Type d = getTexel(y, x);
                if (d == nodata)
                    nodatacount++;
            }
        }
        return float(nodatacount) / (width * height);
    }

    data<Type> generateMipmap()
    {
        data<Type> mipmap(width / 2, height / 2, nodata);

        for (int y = 0; y < height / 2; y++)
        {
            for (int x = 0; x < width / 2; x++)
            {
                Type pixel = area(y * 2, x * 2);
                mipmap.setTexel(pixel, y, x);
            }
        }
        return mipmap;
    }

    template <typename type2>
    data<type2> convert()
    {
        dataCPU<type2> result(width, height, type2(nodata));
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
            {
                Type d = getTexel(y, x);
                if (d == nodata)
                    continue;
                type2 res = type2(d);
                result.setTexel(res, y, x);
            }
        return result;
    }

    Type nodata;
    int width;
    int height;
    int currentDevice;

private:
    Type bilinear(float y, float x) const
    {
        // bilinear interpolation (-2 because the read the next pixel)
        // int _x = std::min(std::max(int(x), 0), texture[lvl].cols-2);
        // int _y = std::min(std::max(int(y), 0), texture[lvl].rows-2);
        if (y > height - 2 || x > width - 2)
            return getTexel(int(y), int(x));

        int _x = int(x);
        int _y = int(y);
        float dx = x - _x;
        float dy = y - _y;

        float weight_tl = (1.0 - dx) * (1.0 - dy);
        float weight_tr = (dx) * (1.0 - dy);
        float weight_bl = (1.0 - dx) * (dy);
        float weight_br = (dx) * (dy);

        Type tl = getTexel(_y, _x);
        Type tr = getTexel(_y, _x + 1);
        Type bl = getTexel(_y + 1, _x);
        Type br = getTexel(_y + 1, _x + 1);

        if (tl == nodata || tr == nodata || bl == nodata || br == nodata)
            return nodata;

        Type pix = Type(tl * weight_tl +
                        tr * weight_tr +
                        bl * weight_bl +
                        br * weight_br);

        return pix;
    }

    Type area(int y, int x) const
    {
        // bilinear interpolation (-2 because the read the next pixel)
        // int _x = std::min(std::max(int(x), 0), texture[lvl].cols-2);
        // int _y = std::min(std::max(int(y), 0), texture[lvl].rows-2);
        if (y > height - 2 || x > width - 2)
            return getTexel(y, x);

        Type tl = getTexel(y, x);
        Type tr = getTexel(y, x + 1);
        Type bl = getTexel(y + 1, x);
        Type br = getTexel(y + 1, x + 1);

        if (tl == nodata || tr == nodata || bl == nodata || br == nodata)
            return nodata;

        Type pix = Type((tl + tr + bl + br) / 4.0f);

        return pix;
    }

    std::unique_ptr<Type> cpu_data;
    GLuint opengl_data;
};

template <typename Type>
class dataMipMap
{
public:
    dataMipMap()
    {
    }

    dataMipMap(int _width, int _height, Type _nodata_value)
    {
        int width = _width;
        int height = _height;
        nodata = _nodata_value;

        while (true)
        {
            dataCPU<Type> lvlData(width, height, nodata);
            data.push_back(lvlData);

            width = int(width / 2);
            height = int(height / 2);

            if (width <= 1 || height <= 1)
                break;
        }
    }

    dataMipMap(const dataCPU<Type> image)
    {
        dataCPU<Type> imageLoD = image;
        nodata = image.nodata;

        while (true)
        {
            data.push_back(imageLoD);
            imageLoD = imageLoD.generateMipmap();

            if (imageLoD.width <= 1 || imageLoD.height <= 1)
                break;
        }
    }

    dataMipMap(const dataMipMapCPU &other)
    {
        nodata = other.nodata;
        for (size_t lvl = 0; lvl < other.data.size(); lvl++)
        {
            data.push_back(other.data[lvl]);
        }
    }

    dataMipMap &operator=(const dataMipMap &other)
    {
        if (this != &other)
        {
            nodata = other.nodata;
            data.clear();

            for (size_t lvl = 0; lvl < other.data.size(); lvl++)
            {
                data.push_back(other.data[lvl]);
            }
        }
        return *this;
    }

    void setToNoData(int lvl)
    {
        data[lvl].set(nodata);
    }

    data<Type> &get(int lvl)
    {
        return data[lvl];
    }

    void generateMipmaps(int baselvl = 0)
    {
        for (size_t lvl = baselvl + 1; lvl < data.size(); lvl++)
        {
            data<Type> d = data[lvl - 1].generateMipmap();
            data[lvl] = d;
        }
    }

    int getLvls()
    {
        return data.size();
    }

    Type nodata;

private:
    std::vector<data<Type>> data;
};