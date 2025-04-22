#pragma once

#include "common/types.h"
#include "xrt/texturefpga.h"
#include "xrt/bufferfpga.h"
#include "xrt/meshfpga.h"

// 2D cross product:  (Ax * By - Ay * Bx)
inline float cross(const Vec2 &a, const Vec2 &b)
{
    return a(0) * b(1) - a(1) * b(0);
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
    return coord(0) * (t[0].*p) +
           coord(1) * (t[1].*p) +
           coord(2) * (t[2].*p);
}

// -----------------------------------------------------------------------------
// BaseRendererCPU
// -----------------------------------------------------------------------------
template <typename InTexType, typename VaryingType, typename OutTexType>
class BaseRendererFPGA
{
public:
    BaseRendererFPGA() = default;
    virtual ~BaseRendererFPGA() = default;

    // Provide your own rendering routine
    void Render(const MeshFPGA &mesh,
                const SE3 &pose,
                const CameraType &cam,
                const TextureFPGA<InTexType> &in_texture,
                TextureFPGA<OutTexType> &out_texture,
                int /*lvl*/)
    {
        Mat4 opencv2opengl = Mat4::Identity();
        opencv2opengl(1, 1) = 1.0;
        opencv2opengl(2, 2) = -1.0;

        Mat4 view_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl * pose.matrix();

        BoundingBox<int> viewport(0, out_texture.width_, 0, out_texture.height_);

        // Loop over triangles
        for (int i = 0; i < mesh.tri_size_; i += 3)
        {
            Vec4 p[3];
            Vec2 t[3]; // if needed

            unsigned int i0 = mesh.ebo_buffer_[i + 0];
            unsigned int i1 = mesh.ebo_buffer_[i + 1];
            unsigned int i2 = mesh.ebo_buffer_[i + 2];

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

    // The pipeline requires two shaders:
    // 1) Vertex shader
    // 2) Fragment shader
    // They must be provided by derived classes.
    virtual void vertex_shader(const Vec4 &inVertex,
                               const Mat4 &tm,
                               Vec4 &gl_Position,
                               VaryingType &outVarying) = 0;

    virtual void fragment_shader(const Vec4 &gl_FragCoord,
                                 const Vec2 &inTexCoord,
                                 const TextureFPGA<InTexType> &inTexture,
                                 const VaryingType &inVarying,
                                 OutTexType &outFragment) = 0;

    // -------------------------------------------------------------------------
    // draw_triangle: minimal CPU rasterizer for one triangle
    // -------------------------------------------------------------------------
    void draw_triangle(const Vec4 *verts,
                       const Vec2 *texcoords,
                       const TextureFPGA<InTexType> &in_texture,
                       const Mat4 &tm,
                       const BoundingBox<int> &viewport,
                       TextureFPGA<OutTexType> &out_texture)
    {
        // Step 1: transform each vertex
        VaryingType perVertex[3];
        Vec4 gl_Position[3];

        for (int i = 0; i < 3; ++i)
        {
            vertex_shader(verts[i], tm, gl_Position[i], perVertex[i]);

            // Perspective divide
            float invW = 1.0f / gl_Position[i](3);
            gl_Position[i](0) *= invW;
            gl_Position[i](1) *= invW;
            gl_Position[i](2) *= invW;
            gl_Position[i](3) = invW;
            // gl_Position[i].w remains 1 or whatever you choose

            // NDC [-1,+1] to pixel coords [0, width], [0, height]
            float x_ndc = 0.5f * (gl_Position[i](0) + 1.0f);
            float y_ndc = 0.5f * (gl_Position[i](1) + 1.0f);

            float x_screen = x_ndc * (float)out_texture.width_;
            float y_screen = y_ndc * (float)out_texture.height_;

            // Clamp to valid pixel range
            x_screen = std::clamp(x_screen, 0.0f, float(out_texture.width_ - 1));
            y_screen = std::clamp(y_screen, 0.0f, float(out_texture.height_ - 1));

            // Overwrite gl_Position with final screen coords
            gl_Position[i](0) = x_screen;
            gl_Position[i](1) = y_screen;
        }

        // Step 2: find triangle bounding box in screen space
        float minX = std::min({gl_Position[0](0), gl_Position[1](0), gl_Position[2](0)});
        float maxX = std::max({gl_Position[0](0), gl_Position[1](0), gl_Position[2](0)});
        float minY = std::min({gl_Position[0](1), gl_Position[1](1), gl_Position[2](1)});
        float maxY = std::max({gl_Position[0](1), gl_Position[1](1), gl_Position[2](1)});

        // Convert to int bounding box
        BoundingBox<int> tri_bb(
            static_cast<int>(std::floor(minX)),
            static_cast<int>(std::ceil(maxX)),
            static_cast<int>(std::floor(minY)),
            static_cast<int>(std::ceil(maxY)));
        // Intersect with the given viewport
        tri_bb.Intersect(viewport);

        // Step 3: compute barycentric denominator
        float denom = 1.0f / triangle_area(
                                 Vec2(gl_Position[0](0), gl_Position[0](1)),
                                 Vec2(gl_Position[1](0), gl_Position[1](1)),
                                 Vec2(gl_Position[2](0), gl_Position[2](1)));

        // Step 4: rasterize each pixel in bounding box
        for (int py = tri_bb.min_y_; py < tri_bb.max_y_; ++py)
        {
            for (int px = tri_bb.min_x_; px < tri_bb.max_x_; ++px)
            {
                Vec4 gl_FragCoord;
                gl_FragCoord(0) = px + 0.5f;
                gl_FragCoord(1) = py + 0.5f;

                // Barycentric coords in 2D
                Vec3 barycentric = denom * Vec3(triangle_area(
                                                    Vec2(gl_FragCoord(0), gl_FragCoord(1)),
                                                    Vec2(gl_Position[1](0), gl_Position[1](1)),
                                                    Vec2(gl_Position[2](0), gl_Position[2](1))),
                                                triangle_area(
                                                    Vec2(gl_Position[0](0), gl_Position[0](1)),
                                                    Vec2(gl_FragCoord(0), gl_FragCoord(1)),
                                                    Vec2(gl_Position[2](0), gl_Position[2](1))),
                                                triangle_area(
                                                    Vec2(gl_Position[0](0), gl_Position[0](1)),
                                                    Vec2(gl_Position[1](0), gl_Position[1](1)),
                                                    Vec2(gl_FragCoord(0), gl_FragCoord(1))));

                // Discard if outside the triangle
                if (barycentric(0) < 0.f || barycentric(1) < 0.f || barycentric(2) < 0.f)
                    continue;

                // Interpolate Z if needed
                gl_FragCoord(2) = barycentric(0) * gl_Position[0](2) +
                                  barycentric(1) * gl_Position[1](2) +
                                  barycentric(2) * gl_Position[2](2);
                gl_FragCoord(3) = barycentric(0) * gl_Position[0](3) +
                                  barycentric(1) * gl_Position[1](3) +
                                  barycentric(2) * gl_Position[2](3);

                // clip fragments to the near/far planes (as if by GL_ZERO_TO_ONE)
                if (gl_FragCoord(2) < 0 || gl_FragCoord(2) > 1)
                    continue;

                // Depth test could go here if you keep a depth buffer

                // Perspective-correct weighting (optional)
                Vec3 perspective = (1 / gl_FragCoord(3)) * Vec3(barycentric(0) * gl_Position[0](3), barycentric(1) * gl_Position[1](3), barycentric(2) * gl_Position[2](3));

                // Interpolate any per-vertex attributes
                Vec2 texcoord = perspective(0) * texcoords[0] +
                                perspective(1) * texcoords[1] +
                                perspective(2) * texcoords[2];

                VaryingType varying = perspective(0) * perVertex[0] +
                                      perspective(1) * perVertex[1] +
                                      perspective(2) * perVertex[2];

                // Run fragment shader
                OutTexType outColor;
                fragment_shader(gl_FragCoord, texcoord, in_texture, varying, outColor);

                out_texture.SetTexel(outColor, py, px);
                // Write out to the color attachment
                // rop(out_texture, px, py, outColor);
            }
        }
    }
};

// -----------------------------------------------------------------------------
// DepthRendererCPU
//   Example derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------
class DepthRendererFPGA
    : public BaseRendererFPGA<float /*InTexType*/, float /*VaryingType*/, float /*OutTexType*/>
{
public:
    DepthRendererFPGA() = default;
    ~DepthRendererFPGA() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec4 &inVertex,
                       const Mat4 &tm,
                       Vec4 &gl_Position,
                       float &outVarying) override
    {
        gl_Position = tm * inVertex;
        // No attributes in outVarying, so do nothing with it
        outVarying = inVertex(2); // example: store Z in outVarying
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Vec2 &inTexCoord,
                         const TextureFPGA<float> &inTexture,
                         const float &inVarying,
                         float &outFragment) override
    {
        // Example: store depth in outFragment as a float:
        // outFragment = gl_FragCoord.z();
        outFragment = inVarying; // use the varying Z from vertex shader
        // Could also do shading or sampling, but here we just store depth
    }
};

// -----------------------------------------------------------------------------
// ImageRendererCPU
//   Another example derived class that might output color
// -----------------------------------------------------------------------------
class ImageRendererFPGA
    : public BaseRendererFPGA<ImageType /*InTexType*/, float /*VaryingType*/, ImageType /*OutTexType*/>
{
public:
    ImageRendererFPGA() = default;
    ~ImageRendererFPGA() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec4 &inVertex,
                       const Mat4 &tm,
                       Vec4 &gl_Position,
                       float &outVarying) override
    {
        gl_Position = tm * inVertex;
        // We are using a "float" for VaryingType, so you can store something if needed
        outVarying = 0.0f; // placeholder
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Vec2 &inTexCoord,
                         const TextureFPGA<ImageType> &inTexture,
                         const float &inVarying,
                         ImageType &outFragment) override
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
