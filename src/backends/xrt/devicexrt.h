#pragma once

#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

xrt::device xrt_device;
xrt::kernel xrt_kernel;

inline bool InitXRT()
{
    std::string binaryFile = "/path/to/binary/file";
    int device_index = 0;

    // std::cout << "Open the device" << device_index << std::endl;
    xrt_device = xrt::device(device_index);
    // std::cout << "Load the xclbin " << binaryFile << std::endl;
    auto uuid = device.load_xclbin(binaryFile);

    xrt_kernel = xrt::kernel(device, uuid, "kernel");
}
