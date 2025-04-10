#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "common/types.h"
#include "common/devicecpu.h"
#include "common/texturecpu.h"
#include "common/buffercpu.h"
#include "common/meshcpu.h"

/*
struct Renderbuffer { int w, h, ys; void *data; };
struct Vert { vec4 position, texcoord, color; };
struct Varying { vec4 texcoord, color; };

void vertex_shader(const Vert &in, vec4 &gl_Position, Varying &OUT) {
    OUT.texcoord = in.texcoord;
    OUT.color = in.color;
    gl_Position = vec4(in.position.x, in.position.y, -2*in.position.z - 2*in.position.w, -in.position.z);
}

void fragment_shader(vec4 &gl_FragCoord, const Varying &IN, vec4 &OUT) {
    OUT = IN.color;
    vec2 wrapped = IN.texcoord.xy - floor(IN.texcoord.xy);
    bool brighter = (wrapped[0] < 0.5) != (wrapped[1] < 0.5);
    if(!brighter)
        OUT.rgb *= 0.5f;
}

// render output unit/render operations pipeline
void rop(Renderbuffer &buf, int x, int y, const vec4 &c) {
    uint8_t *p = (uint8_t*)buf.data + buf.ys*(buf.h - y - 1) + 4*x;
    p[0] = linear_to_srgb8(c[0]);
    p[1] = linear_to_srgb8(c[1]);
    p[2] = linear_to_srgb8(c[2]);
    p[3] = lround(c[3]*255);
}

void draw_triangle(Renderbuffer &color_attachment, const box2 &viewport, const Vert *verts) {
    auto area = [](const vec2 &p0, const vec2 &p1, const vec2 &p2) { return cross(p1 - p0, p2 - p0); };
    auto interpolate = [](const auto a[3], auto p, const vec3 &coord) { return coord.x*a[0].*p + coord.y*a[1].*p + coord.z*a[2].*p; };

    Varying perVertex[3];
    vec4 gl_Position[3];

    box2 aabb = { viewport.hi, viewport.lo };
    for(int i = 0; i < 3; ++i) {
        vertex_shader(verts[i], gl_Position[i], perVertex[i]);

        // convert to normalized device coordinates
        gl_Position[i].w = 1/gl_Position[i].w;
        gl_Position[i].xyz *= gl_Position[i].w;

        // convert to window coordinates
        gl_Position[i].xy = mix(viewport.lo, viewport.hi, 0.5f*(gl_Position[i].xy + 1.0f));
        aabb = join(aabb, gl_Position[i].xy);
    }

    const float denom = 1/area(gl_Position[0].xy, gl_Position[1].xy, gl_Position[2].xy);

    // loop over all pixels in the rectangle bounding the triangle
    const ibox2 iaabb = lround(aabb);
    for(int y = iaabb.lo.y; y < iaabb.hi.y; ++y)
    for(int x = iaabb.lo.x; x < iaabb.hi.x; ++x)
    {
        vec4 gl_FragCoord;
        gl_FragCoord.xy = vec2(x, y) + 0.5f;

        // fragment barycentric coordinates in window coordinates
        const vec3 barycentric = denom*vec3(
            area(gl_FragCoord.xy, gl_Position[1].xy, gl_Position[2].xy),
            area(gl_Position[0].xy, gl_FragCoord.xy, gl_Position[2].xy),
            area(gl_Position[0].xy, gl_Position[1].xy, gl_FragCoord.xy)
        );

        // discard fragment outside the triangle. this doesn't handle edges correctly.
        if(barycentric.x < 0 || barycentric.y < 0 || barycentric.z < 0)
            continue;

        // interpolate inverse depth linearly
        gl_FragCoord.z = interpolate(gl_Position, &vec4::z, barycentric);
        gl_FragCoord.w = interpolate(gl_Position, &vec4::w, barycentric);

        // clip fragments to the near/far planes (as if by GL_ZERO_TO_ONE)
        if(gl_FragCoord.z < 0 || gl_FragCoord.z > 1)
            continue;

        // convert to perspective correct (clip-space) barycentric
        const vec3 perspective = 1/gl_FragCoord.w*barycentric*vec3(gl_Position[0].w, gl_Position[1].w, gl_Position[2].w);

        // interpolate attributes
        Varying varying = {
            interpolate(perVertex, &Varying::texcoord, perspective),
            interpolate(perVertex, &Varying::color, perspective),
        };

        vec4 color;
        fragment_shader(gl_FragCoord, varying, color);
        rop(color_attachment, x, y, color);
    }
}

int main(int argc, char *argv[]) {
    Renderbuffer buffer = { 512, 512, 512*4 };
    buffer.data = calloc(buffer.ys, buffer.h);

    // VAO interleaved attributes buffer
    Vert verts[] = {
        { { -1, -1, -2, 1 }, { 0, 0, 0, 1 }, { 0, 0, 1, 1 } },
        { { 1, -1, -1, 1 }, { 10, 0, 0, 1 }, { 1, 0, 0, 1 } },
        { { 0, 1, -1, 1 }, { 0, 10, 0, 1 }, { 0, 1, 0, 1 } },
    };

    box2 viewport = { 0, 0, buffer.w, buffer.h };
    draw_triangle(buffer, viewport, verts);

    stbi_write_png("out.png", buffer.w, buffer.h, 4, buffer.data, buffer.ys);
}
*/

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
    return coord.x() * (t[0].*p) +
           coord.y() * (t[1].*p) +
           coord.z() * (t[2].*p);
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

    // Provide your own rendering routine
    void Render(const MeshCPU &mesh,
                const SE3 &pose,
                const CameraType &cam,
                const TextureCPU<InTexType> &in_texture,
                TextureCPU<OutTexType> &out_texture,
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
            t[0].x() = mesh.tex_buffer_[i0 * 2 + 0];
            t[0].y() = mesh.tex_buffer_[i0 * 2 + 1];

            t[1].x() = mesh.tex_buffer_[i1 * 2 + 0];
            t[1].y() = mesh.tex_buffer_[i1 * 2 + 1];

            t[2].x() = mesh.tex_buffer_[i2 * 2 + 0];
            t[2].y() = mesh.tex_buffer_[i2 * 2 + 1];

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
                       const BoundingBox<int> &viewport,
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
            gl_Position[i].w() = invW;
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
        BoundingBox<int> tri_bb(
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
                Vec4 gl_FragCoord;
                gl_FragCoord.x() = px + 0.5f;
                gl_FragCoord.y() = py + 0.5f;

                // Barycentric coords in 2D
                Vec3 barycentric = denom * Vec3(triangle_area(
                                                    Vec2(gl_FragCoord.x(), gl_FragCoord.y()),
                                                    Vec2(gl_Position[1].x(), gl_Position[1].y()),
                                                    Vec2(gl_Position[2].x(), gl_Position[2].y())),
                                                triangle_area(
                                                    Vec2(gl_Position[0].x(), gl_Position[0].y()),
                                                    Vec2(gl_FragCoord.x(), gl_FragCoord.y()),
                                                    Vec2(gl_Position[2].x(), gl_Position[2].y())),
                                                triangle_area(
                                                    Vec2(gl_Position[0].x(), gl_Position[0].y()),
                                                    Vec2(gl_Position[1].x(), gl_Position[1].y()),
                                                    Vec2(gl_FragCoord.x(), gl_FragCoord.y())));

                // Discard if outside the triangle
                if (barycentric.x() < 0.f || barycentric.y() < 0.f || barycentric.z() < 0.f)
                    continue;

                // Interpolate Z if needed
                gl_FragCoord.z() = barycentric.x() * gl_Position[0].z() +
                                   barycentric.y() * gl_Position[1].z() +
                                   barycentric.z() * gl_Position[2].z();
                gl_FragCoord.w() = barycentric.x() * gl_Position[0].w() +
                                   barycentric.y() * gl_Position[1].w() +
                                   barycentric.z() * gl_Position[2].w();

                // clip fragments to the near/far planes (as if by GL_ZERO_TO_ONE)
                if (gl_FragCoord.z() < 0 || gl_FragCoord.z() > 1)
                    continue;

                // Depth test could go here if you keep a depth buffer

                // Perspective-correct weighting (optional)
                Vec3 perspective = (1 / gl_FragCoord.w()) * Vec3(barycentric.x() * gl_Position[0].w(), barycentric.y() * gl_Position[1].w(), barycentric.z() * gl_Position[2].w());

                // Interpolate any per-vertex attributes
                Vec2 texcoord = perspective.x() * texcoords[0] +
                                perspective.y() * texcoords[1] +
                                perspective.z() * texcoords[2];

                VaryingType varying = perspective.x() * perVertex[0] +
                                      perspective.y() * perVertex[1] +
                                      perspective.z() * perVertex[2];

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
class DepthRendererCPU
    : public BaseRendererCPU<float /*InTexType*/, float /*VaryingType*/, float /*OutTexType*/>
{
public:
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
        gl_Position = tm * inVertex;
        // No attributes in outVarying, so do nothing with it
        outVarying = inVertex.z(); // example: store Z in outVarying
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Vec2 &inTexCoord,
                         const TextureCPU<float> &inTexture,
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
class ImageRendererCPU
    : public BaseRendererCPU<ImageType /*InTexType*/, float /*VaryingType*/, ImageType /*OutTexType*/>
{
public:
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
        gl_Position = tm * inVertex;
        // We are using a "float" for VaryingType, so you can store something if needed
        outVarying = 0.0f; // placeholder
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Vec2 &inTexCoord,
                         const TextureCPU<ImageType> &inTexture,
                         const float &inVarying,
                         ImageType &outFragment) override
    {
        outFragment = inTexture.Get(inTexCoord.y(), inTexCoord.x());

        // Example: color = [checker pattern], ignoring inVarying
        // float fx = std::floor(gl_FragCoord.x() * 0.1f);
        // float fy = std::floor(gl_FragCoord.y() * 0.1f);
        // bool bright = (static_cast<int>(fx + fy) % 2 == 0);

        // For demonstration, store a grayscale in float
        // outFragment = bright ? 1.0f : 0.2f;
    }
};
