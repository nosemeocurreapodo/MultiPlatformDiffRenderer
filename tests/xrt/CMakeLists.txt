# XRT/FPGA backend tests
find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

add_executable(xrt_tests xrt_tests.cpp)
target_link_libraries(xrt_tests 
    gtest_main 
    ${OpenCV_LIBS}
    Eigen3::Eigen
)
target_include_directories(xrt_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${OpenCV_INCLUDE_DIRS}
)
target_compile_definitions(xrt_tests PRIVATE TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")

gtest_discover_tests(xrt_tests)
