#pragma once

#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

xrt::device device_xrt;
xrt::uuid uuid_xrt;

inline bool InitXRT()
{
    std::string binaryFile = "/path/to/binary/file";
    int device_index = 0;
    device_xrt = xrt::device(device_index);
    uuid_xrt = device_xrt.load_xclbin(binaryFile);
}
