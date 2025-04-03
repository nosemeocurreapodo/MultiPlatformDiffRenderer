#pragma once

#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

template <typename Type>
class dataXRT
{
public:

    dataXRT(int _width, int _height, int _channels, Type _nodata_value)
    {
        nodata = _nodata_value;
        width = _width;
        height = _height;
        channels = _channels;

        xrt::device device(0);

        buffer = xrt::bo(device, sizeof(Type) * channels * height * width);
    }

    dataXRT(const dataXRT &other)
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

    dataXRT &operator=(const dataXRT &other)
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

    Type nodata;
    int width;
    int height;
    int channels;

private:

    xrt::bo buffer;
};
