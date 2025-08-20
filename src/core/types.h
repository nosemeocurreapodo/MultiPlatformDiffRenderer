#pragma once

#include <Eigen/Core>
#include <Eigen/Dense>
// #include <Eigen/Cholesky>
// #include <Eigen/Sparse>
#include <unsupported/Eigen/MatrixFunctions>
#include "sophus/se3.hpp"
#include "sophus/sim3.hpp"

#include "core/camera.h"
#include "core/boundingbox.h"

using Scalar = float;
using Int = int;

template <typename Type, int rows>
using Vec = Eigen::Matrix<Type, rows, 1>;

using Vec2 = Eigen::Matrix<Scalar, 2, 1>;
using Vec3 = Eigen::Matrix<Scalar, 3, 1>;
using Vec4 = Eigen::Matrix<Scalar, 4, 1>;
using Vec5 = Eigen::Matrix<Scalar, 5, 1>;
using Vec6 = Eigen::Matrix<Scalar, 6, 1>;
using Vecx = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>;

using Vec2i = Eigen::Matrix<Int, 2, 1>;
using Vec3i = Eigen::Matrix<Int, 3, 1>;
using Vec4i = Eigen::Matrix<Int, 4, 1>;
using Vecxi = Eigen::Matrix<Int, Eigen::Dynamic, 1>;

template <typename type, int rows, int cols>
using Mat = Eigen::Matrix<type, rows, cols>;

using Mat3 = Eigen::Matrix<Scalar, 3, 3>;
using Mat4 = Eigen::Matrix<Scalar, 4, 4>;
using Mat6 = Eigen::Matrix<Scalar, 6, 6>;
using Matx = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>;

using Mat23 = Eigen::Matrix<Scalar, 2, 3>;
using Mat24 = Eigen::Matrix<Scalar, 2, 4>;

using Quaternion = Eigen::Quaternion<Scalar>;

using SE3 = Sophus::SE3<Scalar>;
using SIM3 = Sophus::Sim3<Scalar>;

using Camera = PinholeCamera<Scalar, Vec2, Vec3, Vec4, Mat4, Mat23, Mat24>;
template <typename Type>
using BoundingBoxType = BoundingBox<Type, Vec2>;

template <typename Type>
inline int getChannels()
{
    if constexpr (std::is_same_v<Type, Scalar>)
    {
        return 1;
    }
    else if constexpr (std::is_same_v<Type, Vec2>)
    {
        return 2;
    }
    else if constexpr (std::is_same_v<Type, Vec3>)
    {
        return 3;
    }
    else if constexpr (std::is_same_v<Type, Vec4>)
    {
        return 4;
    }
    else
    {
        // static_assert(false, "Unsupported type for getChannels");
        // assert(false);
        return 0; // Unreachable
    }
}
