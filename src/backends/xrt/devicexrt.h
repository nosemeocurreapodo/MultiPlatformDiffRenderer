#pragma once

#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

xrt::device device_xrt;
xrt::uuid uuid_xrt;

inline bool InitXRT(std::string xclbin_file, int device_index)
{
    try
    {
        device_xrt = xrt::device(device_index);
        uuid_xrt = device_xrt.load_xclbin(xclbin_file);
        return true;
    }
    catch (const std::exception &e)
    {
        std::cout << e.what() << std::endl;
        return false;
    }
}
