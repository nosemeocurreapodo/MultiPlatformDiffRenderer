#pragma once

#include <Eigen/Core>
#include <Eigen/Dense>
// #include <Eigen/Cholesky>
// #include <Eigen/Sparse>
#include <unsupported/Eigen/MatrixFunctions>
#include "sophus/se3.hpp"
#include "sophus/sim3.hpp"

using RealType = float;

template <typename Type, int rows>
using Vec = Eigen::Matrix<Type, rows, 1>;

using Vec2 = Eigen::Matrix<RealType, 2, 1>;
using Vec3 = Eigen::Matrix<RealType, 3, 1>;
using Vec4 = Eigen::Matrix<RealType, 4, 1>;
using Vec5 = Eigen::Matrix<RealType, 5, 1>;
using Vec6 = Eigen::Matrix<RealType, 6, 1>;
using Vecx = Eigen::Matrix<RealType, Eigen::Dynamic, 1>;

using Vec2i = Eigen::Matrix<int, 2, 1>;
using Vec3i = Eigen::Matrix<int, 3, 1>;
using Vecxi = Eigen::Matrix<int, Eigen::Dynamic, 1>;

template <typename type, int rows, int cols>
using Mat = Eigen::Matrix<type, rows, cols>;

using Mat3 = Eigen::Matrix<RealType, 3, 3>;
using Mat4 = Eigen::Matrix<RealType, 4, 4>;
using Mat6 = Eigen::Matrix<RealType, 6, 6>;
using Matx = Eigen::Matrix<RealType, Eigen::Dynamic, Eigen::Dynamic>;

using Quaternion = Eigen::Quaternion<RealType>;

using SE3 = Sophus::SE3<RealType>;
using SIM3 = Sophus::Sim3<RealType>;

/*
#include "common/lingalglib.h"
#include "common/ldlt_solver.h"

using Vec2f = vec2<float>;
using Vec3f = vec3<float>;
using Vec6f = vec6<float>;
using Vecxf = vecx<float>;

using Mat3f = mat3<float>;
using Mat6f = mat6<float>;
using Matxf = matx<float>;

using Vec2i = vec2<int>;
using Vec3i = vec3<int>;
using Vecxi = vecx<int>;

using SE3f = SE3<float>;

using solverType = LDLT<matxf>;
*/

class PinholeCamera;
class PinholeDistortedCamera;

using CameraType = PinholeCamera;
using ImageType = float;
