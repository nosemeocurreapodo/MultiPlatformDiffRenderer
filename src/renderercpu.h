#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "common/types.h"
#include "common/devicecpu.h"
#include "common/texturecpu.h"
#include "common/buffercpu.h"
#include "common/meshcpu.h"

// 2D cross product:  (Ax * By - Ay * Bx)
inline float cross(const Vec2 &a, const Vec2 &b)
{
    return a(0) * b(1) - a(1) * b(0);
}

// Convert linear float in [0,1] to sRGB8
inline uint8_t linear_to_srgb8(float val)
{
    float x = std::clamp(val, 0.0f, 1.0f);
    return static_cast<uint8_t>(std::lround(x * 255.0f));
}

// Area of a 2D triangle
inline float triangle_area(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2)
{
    return cross(p1 - p0, p2 - p0);
}

// Interpolate a member pointer p across 3 items using barycentric coords
template <typename T, typename MemberPtr>
auto interpolate(const T t[3], MemberPtr p, const Vec3 &coord)
{
    return coord.x * (t[0].*p) +
           coord.y * (t[1].*p) +
           coord.z * (t[2].*p);
}

// -----------------------------------------------------------------------------
// BaseRendererCPU
// -----------------------------------------------------------------------------
template <typename InTexType, typename VaryingType, typename OutTexType>
class BaseRendererCPU
{
public:
    BaseRendererCPU() = default;
    virtual ~BaseRendererCPU() = default;

    // The pipeline requires two shaders:
    // 1) Vertex shader
    // 2) Fragment shader
    // They must be provided by derived classes.
    virtual void vertex_shader(const Vec4 &inVertex,
                               const Mat4 &tm,
                               Vec4 &gl_Position,
                               VaryingType &outVarying) = 0;

    virtual void fragment_shader(const Vec4 &gl_FragCoord,
                                 const TextureCPU<InTexType> &inTexture,
                                 const VaryingType &inVarying,
                                 OutTexType &outFragment) = 0;

    // -------------------------------------------------------------------------
    // draw_triangle: minimal CPU rasterizer for one triangle
    // -------------------------------------------------------------------------
    void draw_triangle(const Vec4 *verts,
                       const Vec2 *texcoords,
                       const TextureCPU<InTexType> &in_texture,
                       const Mat4 &tm,
                       const Window<int> &viewport,
                       TextureCPU<OutTexType> &out_texture)
    {
        // Step 1: transform each vertex
        VaryingType perVertex[3];
        Vec4 gl_Position[3];

        for (int i = 0; i < 3; ++i)
        {
            vertex_shader(verts[i], tm, gl_Position[i], perVertex[i]);

            // Perspective divide
            float invW = 1.0f / gl_Position[i].w();
            gl_Position[i].x() *= invW;
            gl_Position[i].y() *= invW;
            gl_Position[i].z() *= invW;
            // gl_Position[i].w remains 1 or whatever you choose

            // NDC [-1,+1] to pixel coords [0, width], [0, height]
            float x_ndc = 0.5f * (gl_Position[i].x() + 1.0f);
            float y_ndc = 0.5f * (gl_Position[i].y() + 1.0f);

            float x_screen = x_ndc * (float)out_texture.width_;
            float y_screen = y_ndc * (float)out_texture.height_;

            // Clamp to valid pixel range
            x_screen = std::clamp(x_screen, 0.0f, float(out_texture.width_ - 1));
            y_screen = std::clamp(y_screen, 0.0f, float(out_texture.height_ - 1));

            // Overwrite gl_Position with final screen coords
            gl_Position[i].x() = x_screen;
            gl_Position[i].y() = y_screen;
        }

        // Step 2: find triangle bounding box in screen space
        float minX = std::min({gl_Position[0].x(), gl_Position[1].x(), gl_Position[2].x()});
        float maxX = std::max({gl_Position[0].x(), gl_Position[1].x(), gl_Position[2].x()});
        float minY = std::min({gl_Position[0].y(), gl_Position[1].y(), gl_Position[2].y()});
        float maxY = std::max({gl_Position[0].y(), gl_Position[1].y(), gl_Position[2].y()});

        // Convert to int bounding box
        Window<int> tri_bb(
            static_cast<int>(std::floor(minX)),
            static_cast<int>(std::ceil(maxX)),
            static_cast<int>(std::floor(minY)),
            static_cast<int>(std::ceil(maxY)));
        // Intersect with the given viewport
        tri_bb.Intersect(viewport);

        // Step 3: compute barycentric denominator
        float denom = 1.0f / triangle_area(
                                 Vec2(gl_Position[0].x(), gl_Position[0].y()),
                                 Vec2(gl_Position[1].x(), gl_Position[1].y()),
                                 Vec2(gl_Position[2].x(), gl_Position[2].y()));

        // Step 4: rasterize each pixel in bounding box
        for (int py = tri_bb.min_y_; py < tri_bb.max_y_; ++py)
        {
            for (int px = tri_bb.min_x_; px < tri_bb.max_x_; ++px)
            {
                Vec2 fragXY(px + 0.5f, py + 0.5f);

                // Barycentric coords in 2D
                float alpha = triangle_area(
                                  fragXY,
                                  Vec2(gl_Position[1].x(), gl_Position[1].y()),
                                  Vec2(gl_Position[2].x(), gl_Position[2].y())) *
                              denom;
                float beta = triangle_area(
                                 Vec2(gl_Position[0].x(), gl_Position[0].y()),
                                 fragXY,
                                 Vec2(gl_Position[2].x(), gl_Position[2].y())) *
                             denom;
                float gamma = triangle_area(
                                  Vec2(gl_Position[0].x(), gl_Position[0].y()),
                                  Vec2(gl_Position[1].x(), gl_Position[1].y()),
                                  fragXY) *
                              denom;

                // Discard if outside the triangle
                if (alpha < 0.f || beta < 0.f || gamma < 0.f)
                    continue;

                // Interpolate Z if needed
                float fragZ = alpha * gl_Position[0].z() +
                              beta * gl_Position[1].z() +
                              gamma * gl_Position[2].z();
                // Depth test could go here if you keep a depth buffer

                // Perspective-correct weighting (optional)
                float iw0 = 1.0f / gl_Position[0].w();
                float iw1 = 1.0f / gl_Position[1].w();
                float iw2 = 1.0f / gl_Position[2].w();
                float sum = alpha * iw0 + beta * iw1 + gamma * iw2;
                float invSum = (sum != 0.f) ? (1.0f / sum) : 1.0f;

                // Interpolate any per-vertex attributes into a VaryingType struct
                VaryingType varying;
                // Example if your VaryingType had a Vec2 texcoord:
                // varying.texcoord = invSum * (
                //     alpha * iw0 * perVertex[0].texcoord +
                //     beta  * iw1 * perVertex[1].texcoord +
                //     gamma * iw2 * perVertex[2].texcoord
                // );
                // etc.

                // Setup gl_FragCoord
                Vec4 fragCoord;
                fragCoord.x() = (float)px;
                fragCoord.y() = (float)py;
                fragCoord.z() = fragZ;
                fragCoord.w() = 1.0f;

                // Run fragment shader
                OutTexType outColor;
                fragment_shader(fragCoord, in_texture, varying, outColor);

                out_texture.SetTexel(outColor, py, px);
                // Write out to the color attachment
                // rop(out_texture, px, py, outColor);
            }
        }
    }

    // Provide your own rendering routine
    void Render(MeshCPU &mesh,
                const SE3 &pose,
                const CameraType &cam,
                TextureCPU<OutTexType> &buffer,
                int /*lvl*/)
    {
        Mat4 view_matrix = cam.GetProjectiveMatrix(0.1f, 10.0f) * pose.matrix();

        Window<int> viewport(0, buffer.width_, 0, buffer.height_);

        // Loop over triangles
        for (int i = 0; i < mesh.tri_size_; i += 3)
        {
            Vec4 p[3];
            Vec2 t[3]; // if needed

            int i0 = mesh.ebo_buffer_[i + 0];
            int i1 = mesh.ebo_buffer_[i + 1];
            int i2 = mesh.ebo_buffer_[i + 2];

            // Positions
            p[0].x() = mesh.pos_buffer_[i0 * 3 + 0];
            p[0].y() = mesh.pos_buffer_[i0 * 3 + 1];
            p[0].z() = mesh.pos_buffer_[i0 * 3 + 2];
            p[0].w() = 1.0f;

            p[1].x() = mesh.pos_buffer_[i1 * 3 + 0];
            p[1].y() = mesh.pos_buffer_[i1 * 3 + 1];
            p[1].z() = mesh.pos_buffer_[i1 * 3 + 2];
            p[1].w() = 1.0f;

            p[2].x() = mesh.pos_buffer_[i2 * 3 + 0];
            p[2].y() = mesh.pos_buffer_[i2 * 3 + 1];
            p[2].z() = mesh.pos_buffer_[i2 * 3 + 2];
            p[2].w() = 1.0f;

            // Texcoords if needed (example):
            t[0].x() = mesh.tex_buffer_[i0*2 + 0];
            t[0].y() = mesh.tex_buffer_[i0*2 + 1];
            
            t[1].x() = mesh.tex_buffer_[i1*2 + 0];
            t[1].y() = mesh.tex_buffer_[i1*2 + 1];

            t[2].x() = mesh.tex_buffer_[i2*2 + 0];
            t[2].y() = mesh.tex_buffer_[i2*2 + 1];

            // Draw the triangle
            draw_triangle(p, t, mesh.texture_, view_matrix, viewport, buffer);
        }
    }

protected:
    // -------------------------------------------------------------------------
    // rop: "render output pipeline" for writing one RGBA pixel
    // -------------------------------------------------------------------------
    /*
    void rop(TextureCPU<OutTexType> &buf, int x, int y, const OutTexType &c)
    {
        // Here we assume the output is an 8-bit RGBA buffer
        // Adjust as needed if your OutTexType is different
        uint8_t *p = reinterpret_cast<uint8_t *>(buf.data)
                     + buf.ys * (buf.height_ - y - 1)
                     + 4 * x;
        p[0] = linear_to_srgb8(c.x());
        p[1] = linear_to_srgb8(c.y());
        p[2] = linear_to_srgb8(c.z());
        p[3] = static_cast<uint8_t>(
                   std::lround(std::clamp(c.w(), 0.0f, 1.0f) * 255.0f)
               );
    }
    */
};

// -----------------------------------------------------------------------------
// DepthRendererCPU
//   Example derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------
class DepthRendererCPU
    : public BaseRendererCPU<float /*InTexType*/, float /*VaryingType*/, float /*OutTexType*/>
{
public:
    using Base = BaseRendererCPU<float, float, float>;
    DepthRendererCPU() = default;
    ~DepthRendererCPU() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec4 &inVertex,
                       const Mat4 &tm,
                       Vec4 &gl_Position,
                       float &outVarying) override
    {
        Vec4 tm_in = tm * inVertex;
        // Example: invert depth in the range
        gl_Position.x() = tm_in.x();
        gl_Position.y() = tm_in.y();
        gl_Position.z() = -2.f * tm_in.z() - 2.f * tm_in.w();
        gl_Position.w() = -tm_in.w();

        // No attributes in outVarying, so do nothing with it
        outVarying = 0.0f;
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const TextureCPU<float> &inTexture,
                         const float &inVarying,
                         float &outFragment) override
    {
        // Example: store depth in outFragment as a float:
        outFragment = gl_FragCoord.z();
        // Could also do shading or sampling, but here we just store depth
    }
};

// -----------------------------------------------------------------------------
// ImageRendererCPU
//   Another example derived class that might output color
// -----------------------------------------------------------------------------
class ImageRendererCPU
    : public BaseRendererCPU<float /*InTexType*/, float /*VaryingType*/, uchar /*OutTexType*/>
{
public:
    using Base = BaseRendererCPU<float, float, float>;
    ImageRendererCPU() = default;
    ~ImageRendererCPU() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec4 &inVertex,
                       const Mat4 &tm,
                       Vec4 &gl_Position,
                       float &outVarying) override
    {
        Vec4 tm_in = tm * inVertex;
        gl_Position.x() = tm_in.x();
        gl_Position.y() = tm_in.y();
        gl_Position.z() = -2.f * tm_in.z() - 2.f * tm_in.w();
        gl_Position.w() = -tm_in.w();

        // We are using a "float" for VaryingType, so you can store something if needed
        outVarying = 0.0f; // placeholder
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const TextureCPU<float> &inTexture,
                         const float &inVarying,
                         uchar &outFragment) override
    {
        outFragment = inTexture.GetTexel(gl_FragCoord.y(), gl_FragCoord.x());

        // Example: color = [checker pattern], ignoring inVarying
        //float fx = std::floor(gl_FragCoord.x() * 0.1f);
        //float fy = std::floor(gl_FragCoord.y() * 0.1f);
        //bool bright = (static_cast<int>(fx + fy) % 2 == 0);

        // For demonstration, store a grayscale in float
        //outFragment = bright ? 1.0f : 0.2f;
    }
};
