#pragma once

#include "ap_fixed.h"
#include "ap_float.h"
#include "linalgHLS.h"
#include "Posit.h"
#include "core/camera.h"
#include "core/boundingbox.h"

namespace fpga
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
    using Vec = linalgHLS::Mat<Type, rows, 1>;

    using Vec2 = linalgHLS::Vec2<Scalar>;
    using Vec3 = linalgHLS::Vec3<Scalar>;
    using Vec4 = linalgHLS::Vec4<Scalar>;
    using Vec5 = linalgHLS::Vec5<Scalar>;
    using Vec6 = linalgHLS::Vec6<Scalar>;
    // using Vecx = linalg::Vecx<RealType>;

    using Vec2i = linalgHLS::Vec2<Int>;
    using Vec3i = linalgHLS::Vec3<Int>;
    // using Vecxi = linalg::vecx<int>;

    template <typename type, int rows, int cols>
    using Mat = linalgHLS::Mat<type, rows, cols>;

    using Mat3 = linalgHLS::Mat<Scalar, 3, 3>;
    using Mat4 = linalgHLS::Mat<Scalar, 4, 4>;
    using Mat6 = linalgHLS::Mat<Scalar, 6, 6>;

    using Mat23 = linalgHLS::Mat<Scalar, 2, 3>;
    using Mat24 = linalgHLS::Mat<Scalar, 2, 4>;

    // using Matx = linalg::matx<RealType>;

    using Quaternion = linalgHLS::Quaternion<Scalar>;

    using SO3 = linalgHLS::SO3<Scalar>;
    using SE3 = linalgHLS::SE3<Scalar>;
    // using SIM3 = Sim3<RealType>;

    using Camera = PinholeCamera<Scalar, Vec2, Vec3, Vec4, Mat4, Mat23, Mat24>;
    template <typename Type>
    using BoundingBoxType = BoundingBox<Type, Vec2>;
    using ImageType = Scalar;
}

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
