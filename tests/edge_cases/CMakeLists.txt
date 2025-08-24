# Edge case and error handling tests
find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

add_executable(error_handling_tests error_handling_tests.cpp)
target_link_libraries(error_handling_tests 
    gtest_main 
    ${OpenCV_LIBS}
    Eigen3::Eigen
)
target_include_directories(error_handling_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${OpenCV_INCLUDE_DIRS}
)
target_compile_definitions(error_handling_tests PRIVATE TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")

# Add GL support conditionally
if(COMPILE_GL)
    target_link_libraries(error_handling_tests 
        ${OPENGL_LIBRARIES}
        EGL
        glad
    )
    target_compile_definitions(error_handling_tests PRIVATE COMPILE_GL)
endif()

gtest_discover_tests(error_handling_tests)
