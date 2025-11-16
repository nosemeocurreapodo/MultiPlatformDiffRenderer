# Unit tests for individual backends
find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

# Unified typed backend tests
add_executable(renderer_tests renderer_tests.cpp)
target_link_libraries(renderer_tests 
    gtest_main 
    ${OpenCV_LIBS}
    Eigen3::Eigen
)
target_include_directories(renderer_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${OpenCV_INCLUDE_DIRS}
)
target_compile_definitions(renderer_tests PRIVATE TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/src/tests/data")

# Add GL-specific libs/defs when enabled
if(COMPILE_GL)
    target_link_libraries(renderer_tests 
        ${OPENGL_LIBRARIES}
        EGL
        glad
    )
    target_compile_definitions(renderer_tests PRIVATE COMPILE_GL)
endif()

if(COMPILE_HLS)
    target_compile_definitions(renderer_tests PRIVATE COMPILE_HLS)
endif()

gtest_discover_tests(renderer_tests)
