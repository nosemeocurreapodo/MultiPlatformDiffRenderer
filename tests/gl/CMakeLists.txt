find_package(PkgConfig REQUIRED)
pkg_check_modules(EGL REQUIRED egl)
find_package(OpenGL REQUIRED)
#find_package(GLEW REQUIRED)
#find_package(Pangolin REQUIRED)

include_directories(
    ${PROJECT_SOURCE_DIR}/thirdparty/glad
    ${GLEW_INCLUDE_DIRS}
    #${Pangolin_INCLUDE_DIRS}
    )

add_executable(datagl ${PROJECT_SOURCE_DIR}/thirdparty/glad/egl.c ${PROJECT_SOURCE_DIR}/thirdparty/glad/gl.c datagl_test.cpp)
target_compile_features(datagl PUBLIC cxx_std_17)
target_link_libraries(datagl gmock_main ${OpenCV_LIBS}) # ${OPENGL_LIBRARIES} ${EGL_LIBRARIES} ${GLEW_LIBRARIES} ${Pangolin_LIBRARIES})
target_include_directories(datagl PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(datagl PRIVATE TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")
add_test(NAME datagl COMMAND datagl)

add_executable(renderergl ${PROJECT_SOURCE_DIR}/thirdparty/glad/egl.c ${PROJECT_SOURCE_DIR}/thirdparty/glad/gl.c renderergl_test.cpp)
target_compile_features(renderergl PUBLIC cxx_std_17)
target_link_libraries(renderergl gmock_main ${OpenCV_LIBS}) #  ${OPENGL_LIBRARIES} ${EGL_LIBRARIES} ${GLEW_LIBRARIES} ${Pangolin_LIBRARIES})
target_include_directories(renderergl PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(renderergl PRIVATE TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")
add_test(NAME renderergl COMMAND renderergl)
