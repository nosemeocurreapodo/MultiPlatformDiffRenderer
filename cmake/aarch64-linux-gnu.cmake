# cmake/aarch64-linux-gnu.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross compilers from Vitis
set(_TRIPLE aarch64-linux-gnu)
set(_XILINX_VITIS $ENV{XILINX_VITIS})
set(COMMON_IMAGE_ZYNQMP "/home/emanuel/workspace/Xilinx-Platforms/xilinx-zynqmp-common-v2024.2")

set(CMAKE_C_COMPILER   "${_XILINX_VITIS}/gnu/aarch64/lin/aarch64-linux/bin/${_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${_XILINX_VITIS}/gnu/aarch64/lin/aarch64-linux/bin/${_TRIPLE}-g++")

# (Optional but recommended) Point to your Petalinux/SDK sysroot for proper headers/libs:
set(CMAKE_SYSROOT "${COMMON_IMAGE_ZYNQMP}/sysroots/cortexa72-cortexa53-xilinx-linux")
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)