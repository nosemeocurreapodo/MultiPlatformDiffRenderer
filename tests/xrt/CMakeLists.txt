# cmake_minimum_required(VERSION 3.16)
# project(vadd LANGUAGES C CXX)

# ---------- User‑configurable options ---------------------------------------
option(TARGET "Vitis build target (hw, hw_emu, sw_emu…)" "hw")

set(PLATFORM_NAME "xilinx_u280_gen3x16_xdma_1_202211_1" CACHE STRING
    "Name of the Vitis platform (folder name)")
set(PLATFORM
    "/opt/xilinx/platforms/${PLATFORM_NAME}/${PLATFORM_NAME}.xpfm"
    CACHE FILEPATH "Full path to the .xpfm")

# ---------------------------------------------------------------------------
# set(ROOT_DIR ${CMAKE_SOURCE_DIR})
# set(SRC_DIR  ${ROOT_DIR}/src)

# Build‑tree sub‑dirs (mirror the Makefile layout)
set(BUILD_PATH   ${CMAKE_BINARY_DIR})
set(XO_PATH      ${BUILD_PATH}/xo)
set(XCLBIN_PATH  ${BUILD_PATH}/xclbin)
set(HOST_PATH    ${BUILD_PATH}/host)
set(PACKAGE_PATH ${BUILD_PATH}/package)

# ----------------------------------------------------------------------------
# 1) Optional: platforminfo.txt (exactly like the Makefile)
# ----------------------------------------------------------------------------
add_custom_command(
  OUTPUT   ${BUILD_PATH}/platforminfo.txt
  COMMAND  platforminfo  ${PLATFORM} > platforminfo.txt
  WORKING_DIRECTORY ${BUILD_PATH}
  COMMENT "Querying platform information")
add_custom_target(platforminfo DEPENDS ${BUILD_PATH}/platforminfo.txt)

# ----------------------------------------------------------------------------
# 2) Kernel compile: vadd.cpp → vadd.xo
# ----------------------------------------------------------------------------
file(MAKE_DIRECTORY ${XO_PATH})
add_custom_command(
  OUTPUT  ${XO_PATH}/depthrenderfpga.xo
  COMMAND v++ --jobs 12 -c --save-temps
          -t ${TARGET}
          --platform ${PLATFORM}
          -k DepthRenderFPGA
          -o depthrenderfpga.xo
          ${PROJECT_SOURCE_DIR}/src/xrt/rendererfpga.cpp
  WORKING_DIRECTORY ${XO_PATH}
  DEPENDS ${PROJECT_SOURCE_DIR}/src/xrt/rendererfpga.cpp
  COMMENT "Compiling kernel (XO)")
add_custom_target(kernel_xo DEPENDS ${XO_PATH}/depthrenderfpga.xo)

# ----------------------------------------------------------------------------
# 3) Kernel link: vadd.xo → vadd.xclbin
# ----------------------------------------------------------------------------
file(MAKE_DIRECTORY ${XCLBIN_PATH})
add_custom_command(
  OUTPUT  ${XCLBIN_PATH}/depthrenderfpga.xclbin
  COMMAND v++ --jobs 12 -l --save-temps
          -t ${TARGET}
          --platform ${PLATFORM}
          -o depthrenderfpga.xclbin
          ${XO_PATH}/depthrenderfpga.xo
  WORKING_DIRECTORY ${XCLBIN_PATH}
  DEPENDS kernel_xo
  COMMENT "Linking kernel (XCLBIN)")
add_custom_target(xclbin DEPENDS ${XCLBIN_PATH}/depthrenderfpga.xclbin)

# ----------------------------------------------------------------------------
# 4) Host application
# ----------------------------------------------------------------------------
add_executable(rendererxrt_test rendererxrt_test.cpp)

target_include_directories(rendererxrt_test
  PRIVATE $ENV{XILINX_XRT}/include)

target_compile_options(rendererxrt_test
  PRIVATE -Wall -O0 -g -fmessage-length=0)

target_link_directories(rendererxrt_test
  PRIVATE $ENV{XILINX_XRT}/lib)

target_link_libraries(rendererxrt_test
  PRIVATE
    xilinxopencl  # libxilinxopencl.so
    rt
    stdc++
    uuid
    xrt_coreutil
    pthread)

# Place the executable in HOST_PATH (matches Makefile)
set_target_properties(rendererxrt_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${HOST_PATH})

# ----------------------------------------------------------------------------
# 5) Packaging: shell.json + xclbin + host binary
# ----------------------------------------------------------------------------
file(MAKE_DIRECTORY ${PACKAGE_PATH})

add_custom_command(
  OUTPUT  ${PACKAGE_PATH}/_stamp
  # Create shell.json
  COMMAND ${CMAKE_COMMAND} -E echo
          "{\n  \"shell_type\" : \"XRT_FLAT\",\n  \"num_slots\": \"1\"\n}"
          > ${PACKAGE_PATH}/shell.json
  # Copy payload
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${XCLBIN_PATH}/vadd.xclbin  ${PACKAGE_PATH}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:vadd_host>    ${PACKAGE_PATH}
  COMMAND ${CMAKE_COMMAND} -E touch ${PACKAGE_PATH}/_stamp
  DEPENDS xclbin vadd_host
  COMMENT "Packaging shell.json, xclbin and host binary")

add_custom_target(package ALL DEPENDS ${PACKAGE_PATH}/_stamp)

# ----------------------------------------------------------------------------
# 6) Convenience message (shown at the end of a successful build)
# ----------------------------------------------------------------------------
add_custom_target(postmsg ALL
  COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan
      "\nPackage ready in: ${PACKAGE_PATH}\n"
      "Copy it to /lib/firmware/xilinx/ on the target, then run:\n"
      "  xmutil unloadapp\n"
      "  xmutil loadapp vadd\n"
      "  cd /lib/firmware/xilinx/vadd && ./vadd_host ./vadd.xclbin\n"
  DEPENDS package)
