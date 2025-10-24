cmake_minimum_required(VERSION 3.14)

project(MultiPlatformDiffRenderer)

option(COMPILE_CPU "Enable CPU compilation" ON)
option(COMPILE_GL "Enable OpenGL compilation" ON)
option(COMPILE_GLES2 "Enable GLES2 compilation" OFF)
option(COMPILE_HLS "Enable HLS synthesis" OFF)
option(COMPILE_XRT "Enable XRT compilation" OFF)
option(COMPILE_DEMO "Enable Demo compilation" ON)
option(COMPILE_TESTS "Enable Tests compilation" ON)

find_package(Eigen3 REQUIRED)
find_package(OpenCV REQUIRED)

if(NOT TARGET Eigen3::Eigen)
  add_library(Eigen3::Eigen INTERFACE IMPORTED)
  set_target_properties(Eigen3::Eigen PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${EIGEN3_INCLUDE_DIR}")
endif()

set(MultiPlatformRenderer_PATH "src")
set(HLSLInearAlgebra_PATH "HLSLinearAlgebra/include")

include_directories(
    ${MultiPlatformRenderer_PATH}
    ${HLSLInearAlgebra_PATH}
#   ${PROJECT_SOURCE_DIR}/thirdparty/Sophus
    ${PROJECT_SOURCE_DIR}/thirdparty
    ${EIGEN3_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.11.0)

FetchContent_MakeAvailable(googletest)

if(COMPILE_GL AND COMPILE_GLES2)
    message(WARNING "Both GL and GLES2 are selected, plase just pick one, disabling both for nor")
    set(COMPILE_GL OFF)
    set(COMPILE_GLES2 OFF)
endif()

if(COMPILE_GL)
    add_library(glad STATIC
        thirdparty/glad2/src/gl.c
        thirdparty/glad2/src/egl.c
    )
    target_include_directories(glad PUBLIC 
        thirdparty
        thirdparty/glad2/include
    )
    target_include_directories(glad PRIVATE thirdparty)
endif()

# Build glad library
if(COMPILE_GLES2)
    add_library(glad STATIC
        thirdparty/glad2/src/gles2.c
        thirdparty/glad2/src/egl.c
    )
    target_include_directories(glad PUBLIC 
        thirdparty
        thirdparty/glad2/include
    )
    target_include_directories(glad PRIVATE thirdparty)
endif()

# Find required packages for HLS (optional)
if(COMPILE_HLS)
    # Check for Vitis HLS
    if(NOT XILINX_VITIS_PATH AND DEFINED ENV{XILINX_VITIS})
        set(XILINX_VITIS_PATH $ENV{XILINX_VITIS} CACHE PATH "Path to Xilinx Vitis, initialized from ENV{XILINX_VITIS}")
    else()
        set(XILINX_VITIS_PATH "/tools/Xilinx/Vitis/2024.2/" CACHE PATH "Path to Xilinx Vitis, initialized to default")
    endif()

    set(XILINX_VITIS_PATH "${XILINX_VITIS_PATH}" CACHE PATH "Path to the Xilinx Vitis installation directory.")

    #find_program(VITIS_EXECUTABLE vitis
    #    PATHS ${XILINX_VITIS_PATH}/bin
    #    NO_DEFAULT_PATH
    #)

    #if(NOT VITIS_EXECUTABLE)
    #    message(WARNING "Vitis executable not found at ${XILINX_VITIS_PATH}/bin. HLS tests will be disabled.")
    #    set(COMPILE_HLS OFF)
    #else()
    #    message(STATUS "Found Vitis executable: ${VITIS_EXECUTABLE}")
    #endif()

    #find_file(VITIS_SETTINGS_SCRIPT 
    #    NAMES settings64.sh
    #    PATHS ${XILINX_VITIS_PATH}
    #    NO_DEFAULT_PATH
    #    REQUIRED
    #)
    #message(STATUS "Found Vitis settings script: ${VITIS_SETTINGS_SCRIPT}")

    # ---------- Find v++ ----------
    find_program(VPP v++
    HINTS "${XILINX_VITIS_PATH}/bin" "/opt/xilinx/Vitis/bin"
    REQUIRED
    )
    message(STATUS "Found v++ executable: ${VPP}")

    # Output dirs
    set(XO_PATH "${CMAKE_BINARY_DIR}/xo" CACHE PATH "Where to emit .xo")
    file(MAKE_DIRECTORY "${XO_PATH}")

    # ---------- v++ compile: .xo ----------
    set(XO_FILE "${XO_PATH}/${NAME}.xo")

    set(PLATFORM "" CACHE PATH "Xilinx .xpfm for KV260")
    set(TOP_NAME "ImageRendererHLS" CACHE STRING "Kernel top name (-k)")
    set(NAME "imagerendererhls" CACHE STRING "Kernel/file stem name")

    set(VPP_FLAGS "-I${MultiPlatformRenderer_PATH} -I${HLSLInearAlgebra_PATH} -DUSE_VITIS")
    set(TARGET "hw")

    set(MPR_PATH "${CMAKE_SOURCE_DIR}" CACHE PATH "Repo root")
    set(KERNEL_SRC
        "${MPR_PATH}/src/backends/xrt/hls/rendererhls_kernels.cpp"
        CACHE FILEPATH "Kernel source for v++"
    )

    add_custom_command(
    OUTPUT "${XO_FILE}"
    COMMAND "${VPP}" --jobs 12 -c ${VPP_FLAGS} -t ${TARGET}
            --platform "${PLATFORM}" -k "${TOP_NAME}"
            -o "${XO_FILE}" "${KERNEL_SRC}"
    DEPENDS "${KERNEL_SRC}"
    BYPRODUCTS "${XO_FILE}"
    COMMENT "v++: compiling kernel -> ${XO_FILE}"
    VERBATIM
    )

    # Build the kernel by default (remove ALL if you only want it on-demand)
    add_custom_target(kernel_xo ALL DEPENDS "${XO_FILE}")

    # ---------- (Optional) v++ link to .xclbin ----------
    set(XCLBIN_PATH "${CMAKE_BINARY_DIR}/xclbin")
    file(MAKE_DIRECTORY "${XCLBIN_PATH}")
    set(XCLBIN_FILE "${XCLBIN_PATH}/${NAME}.xclbin")
    add_custom_command(
      OUTPUT "${XCLBIN_FILE}"
      COMMAND "${VPP}" -l -t ${TARGET} --platform "${PLATFORM}"
              -o "${XCLBIN_FILE}" "${XO_FILE}"
      DEPENDS "${XO_FILE}"
      BYPRODUCTS "${XCLBIN_FILE}"
      COMMENT "v++: linking -> ${XCLBIN_FILE}"
      VERBATIM
    )
    add_custom_target(xclbin ALL DEPENDS "${XCLBIN_FILE}")

endif()

if(COMPILE_XRT)
    include_directories(
        "/usr/include/xrt"
        "/usr/include/uuid"
        )
endif()

if(COMPILE_DEMO)
    add_subdirectory(src/demo)
endif()

if(COMPILE_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(src/tests)
endif()