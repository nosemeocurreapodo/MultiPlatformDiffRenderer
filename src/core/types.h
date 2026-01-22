#pragma once

// #ifdef USE_EIGEN
#if 0

#include <Eigen/Core>
#include <Eigen/Dense>
// #include <Eigen/Cholesky>
// #include <Eigen/Sparse>
#include <unsupported/Eigen/MatrixFunctions>
#include "sophus/se3.hpp"
#include "sophus/sim3.hpp"

using ImageType = float;
using RealType = float;
using IntType = int;
using PidType = float;

template <typename T>
using Vecx = Eigen::VectorX<T>;

template <typename T, int size>
using Vec = Eigen::Vector<T, size>;
template <typename T>
using Vec2 = Eigen::Matrix<T, 2, 1>;
template <typename T>
using Vec3 = Eigen::Matrix<T, 3, 1>;
template <typename T>
using Vec4 = Eigen::Matrix<T, 4, 1>;
template <typename T>
using Vec5 = Eigen::Matrix<T, 5, 1>;
template <typename T>
using Vec6 = Eigen::Matrix<T, 6, 1>;

template <typename T>
using Matx = Eigen::MatrixX<T>;

template <typename T, int rows, int cols>
using Mat = Eigen::Matrix<T, rows, cols>;
template <typename T>
using Mat3 = Eigen::Matrix<T, 3, 3>;
template <typename T>
using Mat4 = Eigen::Matrix<T, 4, 4>;
template <typename T>
using Mat6 = Eigen::Matrix<T, 6, 6>;

template <typename T>
using Quaternion = Eigen::Quaternion<T>;

template <typename T>
using SO3 = Sophus::SO3<T>;
template <typename T>
using SE3 = Sophus::SE3<T>;

template <typename T, int size>
using LDLT = Eigen::LDLT<Eigen::Matrix<T, size, size>>;

template <typename T>
using LDLTx = Eigen::LDLT<Eigen::MatrixX<T>>;

template <typename T>
using LDLTx_LAPACK = Eigen::LDLT<Eigen::MatrixX<T>>;

#else

#include "linalg/linalg.h"
#include "linalg/linalgx.h"
#include "linalg/ldlt_solver.h"
#include "linalg/ldlt_solverx.h"

#ifdef USE_VITIS
#include "hls_math.h"
#include "ap_int.h"
#include "ap_fixed.h"
// using ImageType = float;
using ImageType = float;
// using RealType = float;
using RealType = half;
//   using RealType = Posit<16, 1>;
// using RealType = ap_fixed<32, 16>;
// using RealType = ap_float<16, 8>;
//  using IntType = int;
//  using IntType = short int;
using IntType = ap_int<16>;
using PidType = float;
#else
using ImageType = float;
using RealType = float;
using IntType = int;
using PidType = float;
#include "linalg/ldlt_solverx_lapack.h"
#endif

template <typename T>
using Vecx = linalg::Vecx<T>;
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

template <typename T>
using Matx = linalg::Matx<T>;
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

template <typename T, int size>
using LDLT = linalg::LDLT<T, size>;

template <typename T>
using LDLTx = linalg::LDLTx<T>;

#ifndef USE_VITIS
template <typename T>
using LDLTx_LAPACK = linalg::LDLT_LAPACK<T>;
#endif

#endif
