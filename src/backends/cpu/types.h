#pragma once

#ifdef USE_EIGEN

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

using Vec2 = Eigen::Matrix<Scalar, 2, 1>;
using Vec3 = Eigen::Matrix<Scalar, 3, 1>;
using Vec4 = Eigen::Matrix<Scalar, 4, 1>;
using Vec5 = Eigen::Matrix<Scalar, 5, 1>;
using Vec6 = Eigen::Matrix<Scalar, 6, 1>;
using Vec7 = Eigen::Matrix<Scalar, 7, 1>;
using Vec8 = Eigen::Matrix<Scalar, 8, 1>;
using Vecx = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>;

using Vec2i = Eigen::Matrix<Int, 2, 1>;
using Vec3i = Eigen::Matrix<Int, 3, 1>;
using Vec4i = Eigen::Matrix<Int, 4, 1>;
using Vec5i = Eigen::Matrix<Int, 5, 1>;
using Vec6i = Eigen::Matrix<Int, 6, 1>;
using Vecxi = Eigen::Matrix<Int, Eigen::Dynamic, 1>;

// template <typename type, int rows, int cols>
// using Mat = Eigen::Matrix<type, rows, cols>;

using Mat3 = Eigen::Matrix<Scalar, 3, 3>;
using Mat4 = Eigen::Matrix<Scalar, 4, 4>;
using Mat6 = Eigen::Matrix<Scalar, 6, 6>;
using Matx = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>;

using Mat23 = Eigen::Matrix<Scalar, 2, 3>;
using Mat24 = Eigen::Matrix<Scalar, 2, 4>;

using Quaternion = Eigen::Quaternion<Scalar>;

using SE3 = Sophus::SE3<Scalar>;
using SIM3 = Sophus::Sim3<Scalar>;

#else
#include "linalg/linalg.h"

using Int = int;
// using Int = short int;
// using Int = ap_int<16>;
using UInt = unsigned int;
// using UInt = unsigned short int;
// using UInt = ap_uint<16>;
using Scalar = float;
// using Scalar = half;
// using Scalar = Posit<16, 1>;
// using Scalar = ap_fixed<32, 16>;
// using Scalar = ap_float<16, 8>;

template <typename T, int rows>
using Vec = linalg::Mat<T, rows, 1>;

using Vec2 = linalg::Vec2<Scalar>;
using Vec3 = linalg::Vec3<Scalar>;
using Vec4 = linalg::Vec4<Scalar>;
using Vec5 = linalg::Vec5<Scalar>;
using Vec6 = linalg::Vec6<Scalar>;
// using Vecx = linalg::Vecx<RealType>;

using Vec2i = linalg::Vec2<Int>;
using Vec3i = linalg::Vec3<Int>;
using Vec4i = linalg::Vec4<Int>;
// using Vecxi = linalg::vecx<int>;

template <typename T, int rows, int cols>
using Mat = linalg::Mat<T, rows, cols>;

using Mat3 = linalg::Mat<Scalar, 3, 3>;
using Mat4 = linalg::Mat<Scalar, 4, 4>;
using Mat6 = linalg::Mat<Scalar, 6, 6>;

using Mat23 = linalg::Mat<Scalar, 2, 3>;
using Mat24 = linalg::Mat<Scalar, 2, 4>;

// using Matx = linalg::matx<RealType>;

using Quaternion = linalg::Quaternion<Scalar>;

using SO3 = linalg::SO3<Scalar>;
using SE3 = linalg::SE3<Scalar>;
// using SIM3 = Sim3<RealType>;

// using Camera = PinholeCamera<Scalar, Vec2, Vec3, Vec4, Mat4, Mat23, Mat24>;
// template <typename Type>
// using BoundingBoxType = BoundingBox<Type, Vec2>;
//  using ImageType = Scalar;

#endif


