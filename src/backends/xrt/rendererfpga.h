#pragma once

#include "typesfpga.h"
#include "texturefpga.h"
#include "bufferfpga.h"
#include "meshfpga.h"

// 2D cross product:  (Ax * By - Ay * Bx)
inline fpga::Scalar cross(const fpga::Vec2 &a, const fpga::Vec2 &b)
{
    return a(0) * b(1) - a(1) * b(0);
}

// Area of a 2D triangle
inline fpga::Scalar triangle_area(const fpga::Vec2 &p0, const fpga::Vec2 &p1, const fpga::Vec2 &p2)
{
    return cross(p1 - p0, p2 - p0);
}

// Interpolate a member pointer p across 3 items using barycentric coords
template <typename T, typename MemberPtr>
auto interpolate(const T t[3], MemberPtr p, const fpga::Vec3 &coord)
{
    return coord(0) * (t[0].*p) +
           coord(1) * (t[1].*p) +
           coord(2) * (t[2].*p);
}

// -----------------------------------------------------------------------------
// BaseRendererCPU
// -----------------------------------------------------------------------------
template <typename InTexType, typename VaryingType, typename OutTexType, typename Derived>
class BaseRendererFPGA
{
public:
    BaseRendererFPGA() = default;
    ~BaseRendererFPGA() = default;

    // Provide your own rendering routine
    void Render(const MeshFPGA &mesh,
                const fpga::SE3 &pose,
                const fpga::Camera &cam,
                const TextureFPGA<InTexType> &in_texture,
                TextureFPGA<OutTexType> &out_texture,
                int /*lvl*/)
    {
        fpga::Mat4 opencv2opengl = fpga::Mat4::Identity();
        opencv2opengl(1, 1) = fpga::Scalar(1);
        opencv2opengl(2, 2) = -fpga::Scalar(1);

        fpga::Mat4 view_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl * pose.matrix();

        const fpga::BoundingBoxType<int> viewport(0, out_texture.width_, 0, out_texture.height_);

    render_triangle_loop:
        for (fpga::Int i = 0; i < mesh.ebo_buffer_.size(); i += 3)
        {
#pragma HLS LOOP_TRIPCOUNT min = 64 max = 64

            fpga::Vec4 p[3];
            fpga::Vec2 t[3]; // if needed

            fpga::UInt i0 = mesh.ebo_buffer_[i + 0];
            fpga::UInt i1 = mesh.ebo_buffer_[i + 1];
            fpga::UInt i2 = mesh.ebo_buffer_[i + 2];

            // Positions
            p[0](0) = mesh.pos_buffer_[i0 * 3 + 0];
            p[0](1) = mesh.pos_buffer_[i0 * 3 + 1];
            p[0](2) = mesh.pos_buffer_[i0 * 3 + 2];
            p[0](3) = 1.0f;

            p[1](0) = mesh.pos_buffer_[i1 * 3 + 0];
            p[1](1) = mesh.pos_buffer_[i1 * 3 + 1];
            p[1](2) = mesh.pos_buffer_[i1 * 3 + 2];
            p[1](3) = 1.0f;

            p[2](0) = mesh.pos_buffer_[i2 * 3 + 0];
            p[2](1) = mesh.pos_buffer_[i2 * 3 + 1];
            p[2](2) = mesh.pos_buffer_[i2 * 3 + 2];
            p[2](3) = 1.0f;

            // Texcoords if needed (example):
            t[0](0) = mesh.tex_buffer_[i0 * 2 + 0];
            t[0](1) = mesh.tex_buffer_[i0 * 2 + 1];

            t[1](0) = mesh.tex_buffer_[i1 * 2 + 0];
            t[1](1) = mesh.tex_buffer_[i1 * 2 + 1];

            t[2](0) = mesh.tex_buffer_[i2 * 2 + 0];
            t[2](1) = mesh.tex_buffer_[i2 * 2 + 1];

            // Draw the triangle
            draw_triangle(p, t, in_texture, view_matrix, viewport, out_texture);
        }
    }

protected:
    inline void call_vertex_shader(const fpga::Vec4 &inVertex,
                                   const fpga::Mat4 &tm,
                                   fpga::Vec4 &gl_Position,
                                   VaryingType &outVarying) const
    {
        static_cast<const Derived *>(this)->vertex_shader(inVertex, tm, gl_Position, outVarying);
    }

    inline void call_fragment_shader(const fpga::Vec4 &gl_FragCoord,
                                     const fpga::Vec2 &inTexCoord,
                                     const TextureFPGA<InTexType> &inTexture,
                                     const VaryingType &inVarying,
                                     OutTexType &outFragment) const
    {
        static_cast<const Derived *>(this)->fragment_shader(
            gl_FragCoord, inTexCoord, inTexture, inVarying, outFragment);
    }

    void draw_triangle(const fpga::Vec4 *verts,
                       const fpga::Vec2 *texcoords,
                       const TextureFPGA<InTexType> &in_texture,
                       const fpga::Mat4 &tm,
                       const fpga::BoundingBoxType<int> &viewport,
                       TextureFPGA<OutTexType> &out_texture) const
    {
        // Step 1: transform each vertex
        VaryingType perVertex[3];
        fpga::Vec4 gl_Position[3];

    draw_vertex_loop:
        for (fpga::Int i = 0; i < 3; ++i)
        {
            call_vertex_shader(verts[i], tm, gl_Position[i], perVertex[i]);

            // Perspective divide
            fpga::Scalar invW = fpga::Scalar(1) / gl_Position[i](3);
            gl_Position[i](0) *= invW;
            gl_Position[i](1) *= invW;
            gl_Position[i](2) *= invW;
            gl_Position[i](3) = invW;
            // gl_Position[i].w remains 1 or whatever you choose

            // NDC [-1,+1] to pixel coords [0, width], [0, height]
            fpga::Scalar x_ndc = fpga::Scalar(0.5) * (gl_Position[i](0) + fpga::Scalar(1));
            fpga::Scalar y_ndc = fpga::Scalar(0.5) * (gl_Position[i](1) + fpga::Scalar(1));

            fpga::Scalar x_screen = x_ndc * fpga::Scalar(out_texture.width_);
            fpga::Scalar y_screen = y_ndc * fpga::Scalar(out_texture.height_);

            // Clamp to valid pixel range
            x_screen = clamp(x_screen, fpga::Scalar(0), fpga::Scalar(out_texture.width_ - 1));
            y_screen = clamp(y_screen, fpga::Scalar(0), fpga::Scalar(out_texture.height_ - 1));

            // Overwrite gl_Position with final screen coords
            gl_Position[i](0) = x_screen;
            gl_Position[i](1) = y_screen;
        }

        // Step 2: find triangle bounding box in screen space

        fpga::BoundingBoxType<int> tri_bb(gl_Position[0].xy(), gl_Position[1].xy(), gl_Position[2].xy());
        // Intersect with the given viewport
        fpga::BoundingBoxType<int> screen_bb = tri_bb.Intersection(viewport);

        // Step 3: compute barycentric denominator
        fpga::Scalar denom = fpga::Scalar(1) / triangle_area(gl_Position[0].xy(), gl_Position[1].xy(), gl_Position[2].xy());

    // Step 4: rasterize each pixel in bounding box
    draw_pixel_loop_y:
        for (fpga::Int py = screen_bb.min_y_; py < screen_bb.max_y_; ++py)
        {
#pragma HLS LOOP_TRIPCOUNT min = 64 max = 64

        draw_pixel_loop_x:
            for (fpga::Int px = screen_bb.min_x_; px < screen_bb.max_x_; ++px)
            {
#pragma HLS LOOP_TRIPCOUNT min = 64 max = 64

                fpga::Vec4 gl_FragCoord;
                gl_FragCoord(0) = fpga::Scalar(px) + fpga::Scalar(0.5);
                gl_FragCoord(1) = fpga::Scalar(py) + fpga::Scalar(0.5);

                // Barycentric coords in 2D
                fpga::Vec3 barycentric = denom * fpga::Vec3(triangle_area(gl_FragCoord.xy(),
                                                                          gl_Position[1].xy(),
                                                                          gl_Position[2].xy()),
                                                            triangle_area(gl_Position[0].xy(),
                                                                          gl_FragCoord.xy(),
                                                                          gl_Position[2].xy()),
                                                            triangle_area(gl_Position[0].xy(),
                                                                          gl_Position[1].xy(),
                                                                          gl_FragCoord.xy()));

                // Discard if outside the triangle
                if (barycentric(0) < fpga::Scalar(0) || barycentric(1) < fpga::Scalar(0) || barycentric(2) < fpga::Scalar(0))
                    continue;

                // Interpolate Z if needed
                gl_FragCoord(2) = barycentric(0) * gl_Position[0](2) +
                                  barycentric(1) * gl_Position[1](2) +
                                  barycentric(2) * gl_Position[2](2);
                gl_FragCoord(3) = barycentric(0) * gl_Position[0](3) +
                                  barycentric(1) * gl_Position[1](3) +
                                  barycentric(2) * gl_Position[2](3);

                // clip fragments to the near/far planes (as if by GL_ZERO_TO_ONE)
                if (gl_FragCoord(2) < fpga::Scalar(0) || gl_FragCoord(2) > fpga::Scalar(1))
                    continue;

                // Depth test could go here if you keep a depth buffer

                // Perspective-correct weighting (optional)
                fpga::Vec3 perspective = (fpga::Scalar(1) / gl_FragCoord(3)) * fpga::Vec3(barycentric(0) * gl_Position[0](3), barycentric(1) * gl_Position[1](3), barycentric(2) * gl_Position[2](3));

                // Interpolate any per-vertex attributes
                fpga::Vec2 texcoord = perspective(0) * texcoords[0] +
                                      perspective(1) * texcoords[1] +
                                      perspective(2) * texcoords[2];

                VaryingType varying = VaryingType(perspective(0) * perVertex[0] +
                                                  perspective(1) * perVertex[1] +
                                                  perspective(2) * perVertex[2]);

                // Run fragment shader
                OutTexType outColor;
                call_fragment_shader(gl_FragCoord, texcoord, in_texture, varying, outColor);

                out_texture.SetTexel(outColor, py, px);
                // Write out to the color attachment
                // rop(out_texture, px, py, outColor);
            }
        }
    }
};

class DepthRendererFPGA
    : public BaseRendererFPGA<fpga::Scalar /*InTexType*/, fpga::Scalar /*VaryingType*/, fpga::Scalar /*OutTexType*/, DepthRendererFPGA>
{
public:
    void vertex_shader(const fpga::Vec4 &inVertex,
                       const fpga::Mat4 &tm,
                       fpga::Vec4 &gl_Position,
                       fpga::Scalar &outVarying) const
    {
        gl_Position = tm * inVertex;
        // No attributes in outVarying, so do nothing with it
        outVarying = inVertex(2); // example: store Z in outVarying
    }

    void fragment_shader(const fpga::Vec4 &gl_FragCoord,
                         const fpga::Vec2 &inTexCoord,
                         const TextureFPGA<fpga::Scalar> &inTexture,
                         const fpga::Scalar &inVarying,
                         fpga::Scalar &outFragment) const
    {
        // Example: store depth in outFragment as a float:
        // outFragment = gl_FragCoord.z();
        outFragment = inVarying; // use the varying Z from vertex shader
        // Could also do shading or sampling, but here we just store depth
    }
};

class ImageRendererFPGA
    : public BaseRendererFPGA<fpga::ImageType /*InTexType*/, fpga::Scalar /*VaryingType*/, fpga::ImageType /*OutTexType*/, ImageRendererFPGA>
{
public:
    void vertex_shader(const fpga::Vec4 &inVertex,
                       const fpga::Mat4 &tm,
                       fpga::Vec4 &gl_Position,
                       fpga::Scalar &outVarying) const
    {
        gl_Position = tm * inVertex;
        // We are using a "float" for VaryingType, so you can store something if needed
        outVarying = 0.0f; // placeholder
    }

    void fragment_shader(const fpga::Vec4 &gl_FragCoord,
                         const fpga::Vec2 &inTexCoord,
                         const TextureFPGA<fpga::ImageType> &inTexture,
                         const fpga::Scalar &inVarying,
                         fpga::ImageType &outFragment) const
    {
        outFragment = inTexture.Get(inTexCoord(1), inTexCoord(0));

        // Example: color = [checker pattern], ignoring inVarying
        // float fx = std::floor(gl_FragCoord.x() * 0.1f);
        // float fy = std::floor(gl_FragCoord.y() * 0.1f);
        // bool bright = (static_cast<int>(fx + fy) % 2 == 0);

        // For demonstration, store a grayscale in float
        // outFragment = bright ? 1.0f : 0.2f;
    }
};
