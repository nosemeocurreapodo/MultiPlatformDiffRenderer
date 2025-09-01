#pragma once

//#include "ap_fixed.h"
//#include "ap_float.h"
#include "linalg/linalg.h"
//#include "Posit.h"
#include "core/camera.h"
#include "core/boundingbox.h"

namespace hls
{
    using Int = int;
    // using Int = short int;
    // using Int = ap_int<16>;
    using UInt = unsigned int;
    // using UInt = unsigned short int;
    // using UInt = ap_uint<16>;
    using Scalar = float;
    //using Scalar = half;
    //using Scalar = Posit<16, 1>;
    //using Scalar = ap_fixed<32, 16>;
    //using Scalar = ap_float<16, 8>;

    template <typename Type, int rows>
    using Vec = linalg::Mat<Type, rows, 1>;

    using Vec2 = linalg::Vec2<Scalar>;
    using Vec3 = linalg::Vec3<Scalar>;
    using Vec4 = linalg::Vec4<Scalar>;
    using Vec5 = linalg::Vec5<Scalar>;
    using Vec6 = linalg::Vec6<Scalar>;
    // using Vecx = linalg::Vecx<RealType>;

    using Vec2i = linalg::Vec2<Int>;
    using Vec3i = linalg::Vec3<Int>;
    // using Vecxi = linalg::vecx<int>;

    template <typename type, int rows, int cols>
    using Mat = linalg::Mat<type, rows, cols>;

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

    using Camera = PinholeCamera<Scalar, Vec2, Vec3, Vec4, Mat4, Mat23, Mat24>;
    template <typename Type>
    using BoundingBoxType = BoundingBox<Type, Vec2>;
    //using ImageType = Scalar;
}

/*
template <int nbits, int ibits>
ap_fixed<nbits, ibits> round(const ap_fixed<nbits, ibits> &p)
{
    return p;
}

template <int nbits, int ibits>
ap_float<nbits, ibits> round(const ap_float<nbits, ibits> &p)
{
    return p;
}

template <int nbits, int ibits>
ap_fixed<nbits, ibits> floor(const ap_fixed<nbits, ibits> &p)
{
    return p;
}

template <int nbits, int ibits>
ap_float<nbits, ibits> floor(const ap_float<nbits, ibits> &p)
{
    return p;
}
*/