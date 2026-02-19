# Ground truth validation tests
find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

add_executable(ground_truth_tests ground_truth_tests.cpp)
target_link_libraries(ground_truth_tests 
    gtest_main 
    ${OpenCV_LIBS}
    Eigen3::Eigen
)
target_include_directories(ground_truth_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${OpenCV_INCLUDE_DIRS}
)
target_compile_definitions(ground_truth_tests PRIVATE TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/src/tests/data")

# Add GL support conditionally
if(COMPILE_GL)
    target_link_libraries(ground_truth_tests 
        ${OPENGL_LIBRARIES}
        EGL
        glad
    )
    target_compile_definitions(ground_truth_tests PRIVATE COMPILE_GL)
endif()

gtest_discover_tests(ground_truth_tests)
