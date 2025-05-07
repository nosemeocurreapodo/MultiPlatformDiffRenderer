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
#include "linalgHLS.h"

using RealType = float;

template <typename Type, int rows>
using Vec = linalgHLS::Mat<Type, rows, 1>;

using Vec2 = linalgHLS::Vec2<RealType>;
using Vec3 = linalgHLS::Vec3<RealType>;
using Vec4 = linalgHLS::Vec4<RealType>;
using Vec5 = linalgHLS::Vec5<RealType>;
using Vec6 = linalgHLS::Vec6<RealType>;
// using Vecx = linalg::Vecx<RealType>;

using Vec2i = linalgHLS::Vec2<int>;
using Vec3i = linalgHLS::Vec3<int>;
// using Vecxi = linalg::vecx<int>;

template <typename type, int rows, int cols>
using Mat = linalgHLS::Mat<type, rows, cols>;

using Mat3 = linalgHLS::Mat<RealType, 3, 3>;
using Mat4 = linalgHLS::Mat<RealType, 4, 4>;
using Mat6 = linalgHLS::Mat<RealType, 6, 6>;
// using Matx = linalg::matx<RealType>;

using Quaternion = linalgHLS::Quaternion<RealType>;

using SE3 = linalgHLS::SE3<RealType>;
// using SIM3 = Sim3<RealType>;
*/

class PinholeCamera;
class PinholeDistortedCamera;

using CameraType = PinholeCamera;
using ImageType = float;
