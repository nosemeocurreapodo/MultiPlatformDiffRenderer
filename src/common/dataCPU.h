#pragma once

template <typename Type>
class dataCPU
{
public:
    dataCPU(int _width, int _height, int _channels, Type _nodata_value)
    {
        nodata = _nodata_value;
        width = _width;
        height = _height;
        channels = _channels;

        m_data = std::make_unique<Type[]>(channels * width * height);

        //set(_nodata_value);
    }

    dataCPU(const dataCPU &other)
    {
        nodata = other.nodata;
        width = other.width;
        height = other.height;
        channels = other.channels;
        m_data = std::make_unique<Type[]>(channels * width * height);

        std::copy(other.m_data.get(), other.m_data.get() + channels * width * height, m_data.get());
    }

    dataCPU &operator=(const dataCPU &other)
    {
        if (this != &other)
        {
            nodata = other.nodata;
            width = other.width;
            height = other.height;
            channels = other.channels;

            m_data = std::make_unique<Type[]>(channels * width * height);
            std::copy(other.m_data.get(), other.m_data.get() + channels * width * height, m_data.get());
        }
        return *this;
    }

    void set(Type *data)
    {
        std::copy(data, data + channels * width * height, m_data.get());
    }

    void toCPU(Type *data)
    {
        std::copy(m_data.get(), m_data.get() + channels * width * height, data);
    }

    /*
    ~data()
    {
        delete[] m_data;
        // m_data = nullptr;
    }
    */

    Type nodata;
    int width;
    int height;
    int channels;

private:
    friend class RendererCPU;

    /*
    void set(const Type value)
    {
        std::fill_n(m_data, width * height, value);
    }
    */

    void setToNoData()
    {
        set(nodata);
    }

    void setTexel(const Type value, int y, int x)
    {
        assert(y >= 0 && x >= 0 && y < height && x < width);

        int address = x + y * width;
        m_data[address] = value;
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

    dataCPU<Type> generateMipmap()
    {
        dataCPU<Type> mipmap(width / 2, height / 2, nodata);

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
    dataCPU<type2> convert()
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

    std::unique_ptr<Type[]> m_data;
};

template <typename Type>
class dataMipMapCPU
{
public:
    dataMipMapCPU()
    {
    }

    dataMipMapCPU(int _width, int _height, Type _nodata_value)
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

    dataMipMapCPU(const dataCPU<Type> image)
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

    dataMipMapCPU(const dataMipMapCPU &other)
    {
        nodata = other.nodata;
        for (size_t lvl = 0; lvl < other.data.size(); lvl++)
        {
            data.push_back(other.data[lvl]);
        }
    }

    dataMipMapCPU &operator=(const dataMipMapCPU &other)
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

    void generateMipmaps(int baselvl = 0)
    {
        for (size_t lvl = baselvl + 1; lvl < data.size(); lvl++)
        {
            dataCPU<Type> d = data[lvl - 1].generateMipmap();
            data[lvl] = d;
        }
    }

    int getLvls()
    {
        return data.size();
    }

    Type nodata;

private:
    std::vector<dataCPU<Type>> data;
};