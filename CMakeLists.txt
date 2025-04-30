cmake_minimum_required(VERSION 3.14)

project(MultiPlatformRenderer)

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
        "HLSLinearAlgebra/src"
        ${PROJECT_SOURCE_DIR}/thirdparty/Sophus
        ${EIGEN3_INCLUDE_DIR}
        ${OpenCV_INCLUDE_DIRS}
        )

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.11.0)

FetchContent_MakeAvailable(googletest)

include(CTest)
enable_testing()

add_subdirectory(tests)
