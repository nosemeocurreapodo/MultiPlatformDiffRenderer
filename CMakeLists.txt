cmake_minimum_required(VERSION 3.14)

project(MultiPlatformDiffRenderer)

option(COMPILE_GL "Enable OpenGL compilation" ON)
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

include(CTest)
enable_testing()

add_subdirectory(tests)
