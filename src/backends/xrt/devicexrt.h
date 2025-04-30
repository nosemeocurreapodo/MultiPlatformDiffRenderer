#pragma once

#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"

xrt::device device_xrt;
xrt::uuid uuid_xrt;
xrt::kernel depth_render_xrt;

inline bool InitXRT()
{
    std::string binaryFile = "/path/to/binary/file";
    device_xrt = xrt::device(device_index);
    uuid_xrt = device.load_xclbin(binaryFile);
}
