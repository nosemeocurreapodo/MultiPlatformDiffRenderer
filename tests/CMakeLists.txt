include(GoogleTest)

add_executable(DepthRenderer Depth/DepthRenderer_test.cpp)
target_compile_features(DepthRenderer PUBLIC cxx_std_17)
target_link_libraries(DepthRenderer gmock_main)
target_include_directories(DepthRenderer PUBLIC ${CMAKE_CURRENT_LIST_DIR})
add_test(NAME DepthRenderer COMMAND DepthRenderer)