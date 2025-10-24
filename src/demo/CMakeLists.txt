#find_package(Eigen3 REQUIRED)
#find_package(OpenCV REQUIRED)
find_package(assimp REQUIRED)
#find_package(Pangolin REQUIRED)

#if(NOT TARGET Eigen3::Eigen)
#  add_library(Eigen3::Eigen INTERFACE IMPORTED)
#  set_target_properties(Eigen3::Eigen PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${EIGEN3_INCLUDE_DIR}")
#endif()

#include_directories("../src")
#include_directories("../tests")
#include_directories("../HLSLinearAlgebra/include")
#include_directories("include")
#include_directories(
#        ${PROJECT_SOURCE_DIR}/thirdparty/Sophus
#        ${EIGEN3_INCLUDE_DIR}
#        ${Pangolin_INCLUDE_DIRS}
#        )

#include_directories( ${CSPARSE_INCLUDE_DIR} ${CHOLMOD_INCLUDE_DIR}) 

add_executable(demo demo_assimp.cpp)
target_link_libraries(demo ${OpenCV_LIBS} ${CMAKE_DL_LIBS} assimp)

if(COMPILE_GL)
  target_compile_definitions(demo PRIVATE COMPILE_GL)
  target_link_libraries(demo EGL glad)
endif()
if(COMPILE_GLES2)
  target_compile_definitions(demo PRIVATE COMPILE_GLES2)
  target_link_libraries(demo EGL glad)
endif()
if(COMPILE_XRT)
  target_compile_definitions(demo PRIVATE COMPILE_XRT)
  target_link_libraries(demo xilinxopencl rt stdc++ uuid xrt_coreutil)
endif()


