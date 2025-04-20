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
using Vec = linalg::Mat<Type, rows, 1>;

using Vec2 = linalg::Vec2<RealType>;
using Vec3 = linalg::Vec3<RealType>;
using Vec4 = linalg::Vec4<RealType>;
using Vec5 = linalg::Vec5<RealType>;
using Vec6 = linalg::Vec6<RealType>;
// using Vecx = linalg::Vecx<RealType>;

using Vec2i = linalg::Vec2<int>;
using Vec3i = linalg::Vec3<int>;
// using Vecxi = linalg::vecx<int>;

template <typename type, int rows, int cols>
using Mat = linalg::Mat<type, rows, cols>;

using Mat3 = linalg::Mat<RealType, 3, 3>;
using Mat4 = linalg::Mat<RealType, 4, 4>;
using Mat6 = linalg::Mat<RealType, 6, 6>;
// using Matx = linalg::matx<RealType>;

using Quaternion = linalg::Quaternion<RealType>;

using SE3 = linalg::SE3<RealType>;
// using SIM3 = Sim3<RealType>;

class PinholeCamera;
class PinholeDistortedCamera;

using CameraType = PinholeCamera;
using ImageType = float;
