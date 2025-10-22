# Performance benchmarking tests
find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

# Try to find Google Benchmark (optional)
find_package(benchmark QUIET)

if(benchmark_FOUND)
    add_executable(benchmark_tests benchmark_tests.cpp)
    target_link_libraries(benchmark_tests 
        gtest_main 
        benchmark::benchmark
        ${OpenCV_LIBS}
        Eigen3::Eigen
    )
    target_include_directories(benchmark_tests PRIVATE 
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/thirdparty
        ${OpenCV_INCLUDE_DIRS}
    )
    target_compile_definitions(benchmark_tests PRIVATE TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")
    target_sources(benchmark_tests PRIVATE ../common/test_framework.cpp)

    # Add GL support conditionally
    if(COMPILE_GL)
        target_link_libraries(benchmark_tests 
            ${OPENGL_LIBRARIES}
            EGL
            glad
        )
        target_compile_definitions(benchmark_tests PRIVATE COMPILE_GL)
    endif()

    gtest_discover_tests(benchmark_tests)
    message(STATUS "Google Benchmark found - benchmark tests enabled")
else()
    message(STATUS "Google Benchmark not found - benchmark tests disabled")
endif()
