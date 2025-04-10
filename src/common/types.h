#pragma once

/*
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
*/

#include "linalg.h"

using RealType = float;

template <typename Type, int rows>
using Vec = vec<Type, rows>;

using Vec2 = vec2<RealType>;
using Vec3 = vec3<RealType>;
using Vec4 = vec4<RealType>;
using Vec5 = vec5<RealType>;
using Vec6 = vec6<RealType>;
using Vecx = vecx<RealType>;

using Vec2i = vec2<int>;
using Vec3i = vec3<int>;
using Vecxi = vecx<int>;

template <typename type, int rows, int cols>
using Mat = mat<type, rows, cols>;

using Mat3 = mat<RealType, 3, 3>;
using Mat4 = mat<RealType, 4, 4>;
using Mat6 = mat<RealType, 6, 6>;
using Matx = matx<RealType>;

//using Quaternion = Eigen::Quaternion<RealType>;

using SE3 = _SE3<RealType>;
// using SIM3 = Sim3<RealType>;

class PinholeCamera;
class PinholeDistortedCamera;

using CameraType = PinholeCamera;
using ImageType = float;
