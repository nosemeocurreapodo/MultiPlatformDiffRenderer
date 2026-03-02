# Integration tests for cross-backend comparison
find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

add_executable(cross_backend_tests cross_backend_tests.cpp)
target_link_libraries(cross_backend_tests 
    gtest_main 
    ${OpenCV_LIBS}
    Eigen3::Eigen
)
target_include_directories(cross_backend_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${OpenCV_INCLUDE_DIRS}
)
target_compile_definitions(cross_backend_tests PRIVATE TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")

# Add GL support conditionally
if(COMPILE_GL)
    target_link_libraries(cross_backend_tests 
        ${OPENGL_LIBRARIES}
        EGL
        glad
    )
    target_compile_definitions(cross_backend_tests PRIVATE COMPILE_GL)
endif()

gtest_discover_tests(cross_backend_tests)
