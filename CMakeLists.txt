cmake_minimum_required(VERSION 3.14)

project(MultiPlatformDiffRenderer)

option(COMPILE_GL "Enable OpenGL compilation" ON)
option(COMPILE_HLS "Enable HLS synthesis" OFF)
option(COMPILE_XRT "Enable XRT compilation" OFF)

find_package(Eigen3 REQUIRED)
find_package(OpenCV REQUIRED)

if(NOT TARGET Eigen3::Eigen)
  add_library(Eigen3::Eigen INTERFACE IMPORTED)
  set_target_properties(Eigen3::Eigen PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${EIGEN3_INCLUDE_DIR}")
endif()

include_directories(
        "src"
        "tests"
        "HLSLinearAlgebra/include"
        ${PROJECT_SOURCE_DIR}/thirdparty/Sophus
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

# Build glad library
if(COMPILE_GL)
    add_library(glad STATIC
        thirdparty/glad/gl.c
        thirdparty/glad/egl.c
    )
    target_include_directories(glad PUBLIC 
        thirdparty
        thirdparty/glad
    )
    target_include_directories(glad PRIVATE thirdparty)
endif()

# Find required packages for HLS (optional)
if(COMPILE_HLS)
    # Check for Vitis HLS
    if(NOT XILINX_VITIS_PATH AND DEFINED ENV{XILINX_VITIS})
        set(XILINX_VITIS_PATH $ENV{XILINX_VITIS} CACHE PATH "Path to Xilinx Vitis, initialized from ENV{XILINX_VITIS}")
    else()
        set(XILINX_VITIS_PATH "/tools/Xilinx/Vitis/2023.2/" CACHE PATH "Path to Xilinx Vitis, initialized to default")
    endif()

    set(XILINX_VITIS_PATH "${XILINX_VITIS_PATH}" CACHE PATH "Path to the Xilinx Vitis installation directory.")

    find_program(VITIS_EXECUTABLE vitis
        PATHS ${XILINX_VITIS_PATH}/bin
        NO_DEFAULT_PATH
    )

    if(NOT VITIS_EXECUTABLE)
        message(WARNING "Vitis executable not found at ${XILINX_VITIS_PATH}/bin. HLS tests will be disabled.")
        set(COMPILE_HLS OFF)
    else()
        message(STATUS "Found Vitis executable: ${VITIS_EXECUTABLE}")
    endif()

endif()

if(COMPILE_HLS AND VITIS_EXECUTABLE)
  find_file(VITIS_SETTINGS_SCRIPT 
      NAMES settings64.sh
      PATHS ${XILINX_VITIS_PATH}
      NO_DEFAULT_PATH
      REQUIRED
  )
  message(STATUS "Found Vitis settings script: ${VITIS_SETTINGS_SCRIPT}")
endif()

include(CTest)
enable_testing()

add_subdirectory(tests)
