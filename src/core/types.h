#pragma once

// #ifdef USE_EIGEN
#if FALSE

#include <Eigen/Core>
#include <Eigen/Dense>
// #include <Eigen/Cholesky>
// #include <Eigen/Sparse>
#include <unsupported/Eigen/MatrixFunctions>
#include "sophus/se3.hpp"
#include "sophus/sim3.hpp"

using Scalar = float;
using Int = int;
using UInt = unsigned int;

template <typename T, int rows>
using Vec = Eigen::Matrix<T, rows, 1>;

template <typename T>
using Vec2 = Eigen::Matrix<T, 2, 1>;
template <typename T>
using Vec3 = Eigen::Matrix<T, 3, 1>;
template <typename T>
using Vec4 = Eigen::Matrix<T, 4, 1>;
template <typename T>
using Vec6 = Eigen::Matrix<T, 6, 1>;

template <typename T>
using Mat3 = Eigen::Matrix<T, 3, 3>;
template <typename T>
using Mat4 = Eigen::Matrix<T, 4, 4>;
template <typename T>
using Mat6 = Eigen::Matrix<T, 6, 6>;

template <typename T>
using Quaternion = Eigen::Quaternion<T>;

template <typename T>
using SE3 = Sophus::SE3<T>;
template <typename T>
using SIM3 = Sophus::Sim3<T>;

#else
#include "linalg/linalg.h"

using ImageType = float;
using RealType = float;
using IntType = int;
using PidType = float;

template <typename T, int size>
using Vec = linalg::Vec<T, size>;
template <typename T>
using Vec2 = linalg::Vec2<T>;
template <typename T>
using Vec3 = linalg::Vec3<T>;
template <typename T>
using Vec4 = linalg::Vec4<T>;
template <typename T>
using Vec5 = linalg::Vec5<T>;
template <typename T>
using Vec6 = linalg::Vec6<T>;

template <typename T, int rows, int cols>
using Mat = linalg::Mat<T, rows, cols>;

template <typename T>
using Mat3 = linalg::Mat3<T>;
template <typename T>
using Mat4 = linalg::Mat4<T>;
template <typename T>
using Mat6 = linalg::Mat6<T>;

template <typename T>
using Quaternion = linalg::Quaternion<T>;

template <typename T>
using SO3 = linalg::SO3<T>;
template <typename T>
using SE3 = linalg::SE3<T>;

#endif
