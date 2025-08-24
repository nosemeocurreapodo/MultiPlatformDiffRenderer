# Unit tests for individual backends
find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

# CPU unit tests
add_executable(cpu_tests cpu_tests.cpp)
target_link_libraries(cpu_tests 
    gtest_main 
    ${OpenCV_LIBS}
    Eigen3::Eigen
)
target_include_directories(cpu_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${OpenCV_INCLUDE_DIRS}
)
target_compile_definitions(cpu_tests PRIVATE TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")

# GL unit tests (conditional)
if(COMPILE_GL)
    add_executable(gl_tests gl_tests.cpp)
    target_link_libraries(gl_tests 
        gtest_main 
        ${OpenCV_LIBS}
        Eigen3::Eigen
        ${OPENGL_LIBRARIES}
        EGL
        glad
    )
    target_include_directories(gl_tests PRIVATE 
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/thirdparty
        ${OpenCV_INCLUDE_DIRS}
    )
    target_compile_definitions(gl_tests PRIVATE TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data" COMPILE_GL)
    gtest_discover_tests(gl_tests)
endif()

gtest_discover_tests(cpu_tests)
