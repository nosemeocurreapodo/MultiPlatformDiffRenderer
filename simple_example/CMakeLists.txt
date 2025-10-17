find_package(Eigen3 REQUIRED)
find_package(OpenCV REQUIRED)
find_package(assimp REQUIRED)
find_package(GLM REQUIRED)
#find_package(Pangolin REQUIRED)

if(NOT TARGET Eigen3::Eigen)
  add_library(Eigen3::Eigen INTERFACE IMPORTED)
  set_target_properties(Eigen3::Eigen PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${EIGEN3_INCLUDE_DIR}")
endif()

include_directories("../src")
include_directories("../tests")
include_directories("../HLSLinearAlgebra/include")
#include_directories("include")
#include_directories(
#        ${PROJECT_SOURCE_DIR}/thirdparty/Sophus
#        ${EIGEN3_INCLUDE_DIR}
#        ${Pangolin_INCLUDE_DIRS}
#        )

#include_directories( ${CSPARSE_INCLUDE_DIR} ${CHOLMOD_INCLUDE_DIR}) 

add_executable(simple_example simple_example.cpp)
target_link_libraries(simple_example ${OpenCV_LIBS} ${CMAKE_DL_LIBS} assimp EGL glad)
