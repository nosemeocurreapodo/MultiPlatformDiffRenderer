#pragma once

#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"


inline bool InitXRT()
{
    std::string binaryFile = "/path/to/binary/file";
    int device_index = 0;

    //std::cout << "Open the device" << device_index << std::endl;
    auto device = xrt::device(device_index);
    //std::cout << "Load the xclbin " << binaryFile << std::endl;
    auto uuid = device.load_xclbin(binaryFile);

    size_t vector_size_bytes = sizeof(int) * DATA_SIZE;

    auto krnl = xrt::kernel(device, uuid, "vadd");
}
