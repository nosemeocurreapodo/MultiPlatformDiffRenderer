include(GoogleTest)

# add_executable(DepthRenderer Depth/DepthRenderer_test.cpp)
# target_compile_features(DepthRenderer PUBLIC cxx_std_17)
# target_link_libraries(DepthRenderer gmock_main)
# target_include_directories(DepthRenderer PUBLIC ${CMAKE_CURRENT_LIST_DIR})
# add_test(NAME DepthRenderer COMMAND DepthRenderer)

add_executable(DataMulti DataMulti/DataMulti_test.cpp)
target_compile_features(DataMulti PUBLIC cxx_std_17)
target_link_libraries(DataMulti gmock_main ${OpenCV_LIBS} ${Pangolin_LIBRARIES})
target_include_directories(DataMulti PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(DataMulti PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
add_test(NAME DataMulti COMMAND DataMulti)