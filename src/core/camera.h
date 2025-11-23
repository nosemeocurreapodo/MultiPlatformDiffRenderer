#pragma once

// #include <cassert>
// #include <vector>
#include "core/types.h"

template <typename T>
class PinholeCamera
{
public:
    PinholeCamera()
    {
        fx_ = 0;
        fy_ = 0;
        cx_ = 0;
        cy_ = 0;
    }
    template <typename T2>
    PinholeCamera(T2 fx, T2 fy, T2 cx, T2 cy, unsigned int width, unsigned int height)
    {
        fx_ = T(fx) / width;
        fy_ = T(fy) / height;
        cx_ = T(cx) / width;
        cy_ = T(cy) / height;

        // float alpha = std::exp(-imageExp(0));
        // float beta = imageExp(1);
        // imageType f_i_cor = alpha * (f_i - beta);
    }
    template <typename T2>
    PinholeCamera(T2 fx, T2 fy, T2 cx, T2 cy)
    {
        fx_ = T(fx);
        fy_ = T(fy);
        cx_ = T(cx);
        cy_ = T(cy);
    }
    PinholeCamera(T *data)
    {
        fx_ = data[0];
        fy_ = data[1];
        cx_ = data[2];
        cy_ = data[3];
    }

    PinholeCamera(const PinholeCamera &other)
    {
        fx_ = other.fx_;
        fy_ = other.fy_;
        cx_ = other.cx_;
        cy_ = other.cy_;
    }

    PinholeCamera &operator=(const PinholeCamera &other)
    {
        if (this != &other)
        {
            fx_ = other.fx_;
            fy_ = other.fy_;
            cx_ = other.cx_;
            cy_ = other.cy_;
        }
        return *this;
    }

    Mat4<T> GetProjectiveMatrix(T znear, T zfar) const
    {
        Mat4<T> projmat; // = linalg::Mat4<T>::Zero();

        projmat(0, 0) = T(2) * fx_;
        projmat(0, 1) = T(0);
        projmat(0, 2) = T(1) - T(2) * cx_;
        projmat(0, 3) = T(0);
        //- here to flip the y axis (to render like opencv)
        projmat(1, 0) = T(0);
        projmat(1, 1) = -T(2) * fy_;
        projmat(1, 2) = -T(1) + T(2) * cy_;
        projmat(1, 3) = T(0);

        projmat(2, 0) = T(0);
        projmat(2, 1) = T(0);
        projmat(2, 2) = -(zfar + znear) / (zfar - znear);
        projmat(2, 3) = -T(2) * zfar * znear / (zfar - znear);

        projmat(3, 0) = T(0);
        projmat(3, 1) = T(0);
        projmat(3, 2) = -T(1);
        projmat(3, 3) = T(0);

        /*
        projmat(0, 0) = T(2) * fx_;
        projmat(1, 1) = -T(2) * fy_;
        projmat(0, 2) = T(2) * cx_ - T(1);
        projmat(1, 2) = T(1) - T(2) * cy_;
        projmat(2, 2) = (zfar + znear) / (zfar - znear);
        projmat(3, 2) = T(1);
        projmat(2, 3) = -T(2) * zfar * znear / (zfar - znear);
        */
        /*
        T l = -znear * cx_ / fx_;
        T r = znear * (T(1) - cx_) / fx_;
        T t = znear * (T(1) - cy_) / fy_;
        T b = -znear * cy_ / fy_;

        projmat(0, 0) = T(2) * znear / (r - l);
        projmat(1, 1) = T(2) * znear / (t - b);
        projmat(0, 2) = (r + l) / (r - l);
        projmat(1, 2) = (t + b) / (t - b);
        projmat(2, 2) = -(zfar + znear) / (zfar - znear);
        projmat(3, 2) = -T(1);
        projmat(2, 3) = -T(2) * zfar * znear / (zfar - znear);
        */

        return projmat;
    }

    bool IsPixVisible(linalg::Vec2<T> pix) const
    {
        // the idea here is that if we have 3 pixels
        // the first goes from 0 to 1, the second 1 to 2, the third 2 to 3, and the forth from 3 to 4
        // so here the max is one more than the last pixel
        // if (pix(0) < window_min_x || pix(0) > window_max_x || pix(1) < window_min_y || pix(1) > window_max_y)
        //    return false;
        if (pix(0) < T(0) || pix(0) > T(1) || pix(1) < T(0) || pix(1) > T(1))
            return false;
        return true;
    }

    /*
    vec2f pointToPix(vec3f point)
    {
        vec2f pix;
        pix(0) = fx * point(0) / point(2) + cx;
        pix(1) = fy * point(1) / point(2) + cy;
        return pix;
    }
    */

    Vec2<T> RayToPix(Vec3<T> ray) const
    {
        linalg::Vec2<T> pix;
        pix(0) = fx_ * ray(0) + cx_;
        pix(1) = fy_ * ray(1) + cy_;
        return pix;
        // return vec2<float>(fx * ray(0) + cx, fy * ray(1) + cy);
    }

    Mat<T, 2, 3> d_pix_d_ver(Vec3<T> ver) const
    {
        Mat<T, 2, 3> d_pix_d_ver;

        d_pix_d_ver(0, 0) = fx_ / ver(2);
        d_pix_d_ver(0, 1) = T(0);
        d_pix_d_ver(0, 2) = -fx_ * ver(0) / (ver(2) * ver(2));

        d_pix_d_ver(1, 0) = T(0);
        d_pix_d_ver(1, 1) = fy_ / ver(2);
        d_pix_d_ver(1, 2) = -fy_ * ver(1) / (ver(2) * ver(2));

        return d_pix_d_ver;
    }

    Mat<T, 2, 4> d_pix_d_intrinsics(Vec3<T> ray) const
    {
        Mat<T, 2, 4> d_pix_d_int;

        d_pix_d_int(0, 0) = ray(0);
        d_pix_d_int(0, 1) = T(0);
        d_pix_d_int(0, 2) = T(1);
        d_pix_d_int(0, 3) = T(0);

        d_pix_d_int(1, 0) = T(0);
        d_pix_d_int(1, 1) = ray(1);
        d_pix_d_int(1, 2) = T(0);
        d_pix_d_int(1, 3) = T(1);

        return d_pix_d_int;
    }

    Vec3<T> PixToRay(Vec2<T> pix) const
    {
        Vec3<T> ray;
        ray(0) = (pix(0) - cx_) / fx_;
        ray(1) = (pix(1) - cy_) / fy_;
        ray(2) = T(1);
        return ray;
    }

    /*
    mat<float, 3, 4> d_ray_d_intrinsics(vec2f pix)
    {
        mat<float, 3, 4> d_ray_d_int;

        d_ray_d_int(0, 0) = -(pix(0) - cx) / (fx * fx);
        d_ray_d_int(0, 1) = 0.0;
        d_ray_d_int(0, 2) = -1.0 / fx;
        d_ray_d_int(0, 3) = 0.0;

        d_ray_d_int(1, 0) = 0.0;
        d_ray_d_int(1, 1) = -(pix(1) - cy) / (fy * fy);
        d_ray_d_int(1, 2) = 0.0;
        d_ray_d_int(1, 3) = -1.0 / fy;

        d_ray_d_int(2, 0) = 0.0;
        d_ray_d_int(2, 1) = 0.0;
        d_ray_d_int(2, 2) = 0.0;
        d_ray_d_int(2, 3) = 0.0;

        return d_ray_d_int;
    }
    */

    Vec4<T> GetParams() const
    {
        return Vec4<T>(fx_, fy_, cx_, cy_);
    }

    void SetParams(Vec4<T> params)
    {
        fx_ = params(0);
        fy_ = params(1);
        cx_ = params(2);
        cy_ = params(3);
    }

    /*
    bool operator==(pinholeCamera c)
    {
        if (fx == c.fx && fy == c.fy && cx == c.cx && cy == c.cy)
            return true;
        return false;
    }
    */

private:
    T fx_;
    T fy_;
    T cx_;
    T cy_;
};

/*
template <typename Type>
class PinholeDistortedCamera
{
public:
    PinholeDistortedCamera(float _fx, float _fy, float _cx, float _cy, float _k1, int _width, int _height)
    {
        fx_ = _fx / _width;
        fy_ = _fy / _height;
        cx_ = _cx / _width;
        cy_ = _cy / _height;
        k1_ = _k1;
        // k2 = 0.0;
        // p1 = 0.0;
        // p2 = 0.0;
        // k3 = 0.0;
    }

    PinholeDistortedCamera(float _fx, float _fy, float _cx, float _cy, int _width, int _height)
    {
        fx_ = _fx / _width;
        fy_ = _fy / _height;
        cx_ = _cx / _width;
        cy_ = _cy / _height;
        k1_ = 0.0;
        // k2 = 0.0;
        // p1 = 0.0;
        // p2 = 0.0;
        // k3 = 0.0;
    }

    PinholeDistortedCamera(const PinholeDistortedCamera &other)
    {
        fx_ = other.fx_;
        fy_ = other.fy_;
        cx_ = other.cx_;
        cy_ = other.cy_;
        k1_ = other.k1_;
    }

    PinholeDistortedCamera &operator=(const PinholeDistortedCamera &other)
    {
        if (this != &other)
        {
            fx_ = other.fx_;
            fy_ = other.fy_;
            cx_ = other.cx_;
            cy_ = other.cy_;
            k1_ = other.k1_;
        }
        return *this;
    }

    template <typename Vec2Type>
    bool IsPixVisible(Vec2Type pix)
    {
        // the idea here is that if we have 3 pixels
        // the first goes from 0 to 1, the second 1 to 2, the third 2 to 3, and the forth from 3 to 4
        // so here the max is one more than the last pixel
        // if (pix(0) < window_min_x || pix(0) > window_max_x || pix(1) < window_min_y || pix(1) > window_max_y)
        //    return false;
        if (pix(0) < 0 || pix(0) > 1.0 || pix(1) < 0 || pix(1) > 1.0)
            return false;
        return true;
    }

    template <typename Vec2Type, typename Vec3Type>
    Vec2Type RayToPix(Vec3Type ray)
    {
        Vec3Type dist_ray = DistortRay(ray);
        Vec2Type pixel(fx_ * dist_ray(0) + cx_, fy_ * dist_ray(1) + cy_);
        return pixel;
    }

    //mat<float, 2, 3> d_pix_d_ver(vec3f ver)
    //{
    //    mat<float, 2, 3> d_pix_d_ver;

    //    d_pix_d_ver(0, 0) = fx / ver(2);
    //    d_pix_d_ver(0, 1) = 0;
    //    d_pix_d_ver(0, 2) = -fx * ver(0) / (ver(2) * ver(2));

    //    d_pix_d_ver(1, 0) = 0;
    //    d_pix_d_ver(1, 1) = fy / ver(2);
    //    d_pix_d_ver(1, 2) = -fy * ver(1) / (ver(2) * ver(2));

    //    return d_pix_d_ver;
    //}


    template <typename MatType, typename Vec3Type>
    MatType<RealType, 2, 3> d_pix_d_ver(Vec3Type ver)
    {
        Vec3Type ray = ver / ver(2);

        MatType<Type, 2, 3> d_pix_d_dist = d_pix_d_distRay();
        MatType<Type, 3, 3> d_dis_d_ray = d_distray_d_ray(ray);
        MatType<Type, 3, 3> d_ray_d_v = d_ray_d_ver(ver);

        MatType<RealType, 2, 3> d_pix_d_ray = d_pix_d_dist * d_dis_d_ray;
        MatType<RealType, 2, 3> d_pix_d_v = d_pix_d_ray * d_ray_d_v;

        return d_pix_d_v;
    }

    template <typename MatType, typename Vec3Type>
    MatType<RealType, 2, 5> d_pix_d_intrinsics(Vec3Type ray)
    {
        Mat<RealType, 2, 5> d_pix_d_int;

        Vec3 dist_ray = DistortRay(ray);
        float r2 = ray(0) * ray(0) + ray(1) * ray(1);

        d_pix_d_int(0, 0) = dist_ray(0);
        d_pix_d_int(0, 1) = 0;
        d_pix_d_int(0, 2) = 1.0;
        d_pix_d_int(0, 3) = 0;
        d_pix_d_int(0, 4) = fx_ * ray(0) * r2;
        // d_pix_d_int(0, 5) = fx * 2 * ray(0) * ray(1);

        d_pix_d_int(1, 0) = 0;
        d_pix_d_int(1, 1) = dist_ray(1);
        d_pix_d_int(1, 2) = 0;
        d_pix_d_int(1, 3) = 1.0;
        d_pix_d_int(1, 4) = fy_ * ray(1) * r2;
        // d_pix_d_int(1, 5) = fy * (r2 + 2 * ray(1) * ray(1));

        return d_pix_d_int;
    }

    template <typename Vec3Type, typename Vec2Type>
    Vec3Type PixToRay(Vec2Type pix)
    {
        Vec3Type dist_ray;
        dist_ray(0) = (pix(0) - cx_) / fx_;
        dist_ray(1) = (pix(1) - cy_) / fy_;
        dist_ray(2) = 1.0;

        Vec3Type ray = CorrectRay(dist_ray);

        return ray;
    }

    //mat<float, 3, 5> d_ray_d_intrinsics(vec2f pix)
    //{
    //    // Step 2: Compute radius squared from the optical axis
        float r2 = ray(0) * ray(0) + ray(1) * ray(1);
        // float r4 = r2 * r2;
        // float r6 = r4 * r2;

        // Step 3: Compute the radial distortion factor
    //    float radial = 1 + k1 * r2; // + k2 * r4 + k3 * r6;

        // Step 4: Apply radial and tangential distortion
    //    float xDistorted = ray(0) * radial; // + 2 * p1 * ray(0) * ray(1);        // + p2 * (r2 + 2 * ray(0) * ray(0));
    //    float yDistorted = ray(1) * radial; // + p1 * (r2 + 2 * ray(1) * ray(1)); // + 2 * p2 * ray(0) * ray(1);

    //    mat<float, 3, 5> d_ray_d_int;

    //    d_ray_d_int(0, 0) = -(pix(0) - cx) / (fx * fx);
    //    d_ray_d_int(0, 1) = 0.0;
    //    d_ray_d_int(0, 2) = -1.0 / fx;
    //    d_ray_d_int(0, 3) = 0.0;
    //    d_ray_d_int(0, 4) = 0.0;

    //    d_ray_d_int(1, 0) = 0.0;
    //    d_ray_d_int(1, 1) = -(pix(1) - cy) / (fy * fy);
    //    d_ray_d_int(1, 2) = 0.0;
    //    d_ray_d_int(1, 3) = -1.0 / fy;
    //    d_ray_d_int(1, 4) = 0.0;

    //    d_ray_d_int(2, 0) = 0.0;
    //    d_ray_d_int(2, 1) = 0.0;
    //    d_ray_d_int(2, 2) = 0.0;
    //    d_ray_d_int(2, 3) = 0.0;
    //    d_ray_d_int(2, 4) = 0.0;

    //    return d_ray_d_int;
    //}

    template <typename Vec5Type>
    Vec5Type GetParams()
    {
        return Vec5Type(fx_, fy_, cx_, cy_, k1_);
    }

    template <typename Vec5Type>
    void setParams(Vec5Type params)
    {
        fx_ = params(0);
        fy_ = params(1);
        cx_ = params(2);
        cy_ = params(3);
        k1_ = params(4);
        // p1 = params(5);
    }

    //bool operator==(cameraDist c)
    //{
    //    if (fx == c.fx && fy == c.fy && cx == c.cx && cy == c.cy)
    //        return true;
    //    return false;
    //}


private:
    Vec3 DistortRay(Vec3 ray)
    {
        // Step 2: Compute radius squared from the optical axis
        float r2 = ray(0) * ray(0) + ray(1) * ray(1);
        // float r4 = r2 * r2;
        // float r6 = r4 * r2;

        // Step 3: Compute the radial distortion factor
        float radial = 1 + k1_ * r2; // + k2 * r4 + k3 * r6;

        // Step 4: Apply radial and tangential distortion
        float xDistorted = ray(0) * radial; // + 2 * p1 * ray(0) * ray(1);        // + p2 * (r2 + 2 * ray(0) * ray(0));
        float yDistorted = ray(1) * radial; // + p1 * (r2 + 2 * ray(1) * ray(1)); // + 2 * p2 * ray(0) * ray(1);

        // Step 5: Convert to pixel coordinates using the focal length and principal point
        Vec3 distRay(xDistorted, yDistorted, 1.0);

        return distRay;
    }

    Mat<RealType, 2, 3> d_pix_d_distRay()
    {
        Mat<RealType, 2, 3> d_pix_d_dist;

        d_pix_d_dist(0, 0) = fx_;
        d_pix_d_dist(0, 1) = 0;
        d_pix_d_dist(0, 2) = 1.0;

        d_pix_d_dist(1, 0) = 0;
        d_pix_d_dist(1, 1) = fy_;
        d_pix_d_dist(1, 2) = 1.0;

        return d_pix_d_dist;
    }

    Mat3 d_distray_d_ray(Vec3 ray)
    {
        Mat3 d_dist_d_ray;

        RealType r2 = ray(0) * ray(0) + ray(1) * ray(1);
        RealType radial = 1.0 + k1_ * r2;

        d_dist_d_ray(0, 0) = radial + ray(0) * k1_ * ray(0);
        d_dist_d_ray(0, 1) = 0.0;
        d_dist_d_ray(0, 2) = 0.0;

        d_dist_d_ray(1, 0) = 0.0;
        d_dist_d_ray(1, 1) = radial + ray(1) * k1_ * ray(1);
        d_dist_d_ray(1, 2) = 0.0;

        d_dist_d_ray(2, 0) = 0.0;
        d_dist_d_ray(2, 1) = 0.0;
        d_dist_d_ray(2, 2) = 0.0;

        return d_dist_d_ray;
    }

    Mat3 d_ray_d_ver(Vec3 ver)
    {
        Mat3 d_ray_d_v;

        d_ray_d_v(0, 0) = 1.0 / ver(2);
        d_ray_d_v(0, 1) = 0.0;
        d_ray_d_v(0, 2) = -ver(0) / (ver(2) * ver(2));

        d_ray_d_v(1, 0) = 0.0;
        d_ray_d_v(1, 1) = 1.0 / ver(2);
        d_ray_d_v(1, 2) = -ver(1) / (ver(2) * ver(2));

        d_ray_d_v(2, 0) = 0.0;
        d_ray_d_v(2, 1) = 0.0;
        d_ray_d_v(2, 2) = 0.0;

        return d_ray_d_v;
    }

    Vec3 CorrectRay(Vec3 distRay)
    {
        Vec3 cRay = distRay;
        const int maxIterations = 5; // Number of iterations (adjust if needed)
        for (int i = 0; i < maxIterations; i++)
        {
            float r2 = cRay(0) * cRay(0) + cRay(1) * cRay(1);
            // float r4 = r2 * r2;
            // float r6 = r4 * r2;
            //  Radial distortion factor.
            float radial = 1 + k1_ * r2; // + k2 * r4 + k3 * r6;
            // Tangential distortion components.
            float deltaX = 0.0; // 2 * p1 * cRay(0) * cRay(1) + p2 * (r2 + 2 * cRay(0) * cRay(0));
            float deltaY = 0.0; // p1 * (r2 + 2 * cRay(1) * cRay(1)) + 2 * p2 * cRay(0) * cRay(1);

            // Compute the undistorted estimate from the distorted normalized coordinates.
            cRay(0) = (distRay(0) - deltaX) / radial;
            cRay(1) = (distRay(1) - deltaY) / radial;
        }
        return cRay;
    }

    float fx_;
    float fy_;
    float cx_;
    float cy_;

    float k1_;
    // float k2;
    // float p1;
    // float p2;
    // float k3;
};
*/
