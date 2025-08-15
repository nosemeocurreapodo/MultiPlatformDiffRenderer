#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "backends/cpu/devicecpu.h"
#include "backends/cpu/texturecpu.h"
#include "backends/cpu/buffercpu.h"
#include "backends/cpu/meshcpu.h"

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
inline float cross(const cpu::Vec2 &a, const cpu::Vec2 &b)
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
inline float triangle_area(const cpu::Vec2 &p0, const cpu::Vec2 &p1, const cpu::Vec2 &p2)
{
    return cross(p1 - p0, p2 - p0);
}

// Interpolate a member pointer p across 3 items using barycentric coords
template <typename T, typename MemberPtr>
auto interpolate(const T t[3], MemberPtr p, const cpu::Vec3 &coord)
{
    return coord(0) * (t[0].*p) +
           coord(1) * (t[1].*p) +
           coord(2) * (t[2].*p);
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
                const cpu::SE3 &pose,
                const cpu::Camera &cam,
                const TextureCPU<InTexType> &in_texture,
                TextureCPU<OutTexType> &out_texture,
                int in_lvl,
                int out_lvl)
    {
        in_nodata_ = in_texture.nodata();

        // out_texture.fill(out_texture.nodata());
        for (int i = 0; i < out_texture.size(); ++i)
        {
            out_texture.data_[i] = out_texture.nodata();
        }

        cpu::Mat4 opencv2opengl = cpu::Mat4::Identity();
        opencv2opengl(1, 1) = 1.0;
        opencv2opengl(2, 2) = -1.0;

        cpu::Mat4 view_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl;
        cpu::Mat4 pose_matrix = pose.matrix();

        fx = cam.GetParams()(0);
        fy = cam.GetParams()(1);

        cpu::BoundingBoxType<int> viewport(0, out_texture.width() - 1, 0, out_texture.height() - 1);

        // Loop over triangles
        for (int i = 0; i < mesh.GetEboBuffer().size(); i += 3)
        {
            cpu::Vec3 p[3];
            cpu::Vec2 t[3]; // if needed
            float w[3];     // if needed

            unsigned int i0 = mesh.GetEboBuffer()[i + 0];
            unsigned int i1 = mesh.GetEboBuffer()[i + 1];
            unsigned int i2 = mesh.GetEboBuffer()[i + 2];

            // Positions
            p[0](0) = mesh.GetPosBuffer()[i0 * 3 + 0];
            p[0](1) = mesh.GetPosBuffer()[i0 * 3 + 1];
            p[0](2) = mesh.GetPosBuffer()[i0 * 3 + 2];

            p[1](0) = mesh.GetPosBuffer()[i1 * 3 + 0];
            p[1](1) = mesh.GetPosBuffer()[i1 * 3 + 1];
            p[1](2) = mesh.GetPosBuffer()[i1 * 3 + 2];

            p[2](0) = mesh.GetPosBuffer()[i2 * 3 + 0];
            p[2](1) = mesh.GetPosBuffer()[i2 * 3 + 1];
            p[2](2) = mesh.GetPosBuffer()[i2 * 3 + 2];

            // Texcoords if needed (example):
            t[0](0) = mesh.GetTexBuffer()[i0 * 2 + 0];
            t[0](1) = mesh.GetTexBuffer()[i0 * 2 + 1];

            t[1](0) = mesh.GetTexBuffer()[i1 * 2 + 0];
            t[1](1) = mesh.GetTexBuffer()[i1 * 2 + 1];

            t[2](0) = mesh.GetTexBuffer()[i2 * 2 + 0];
            t[2](1) = mesh.GetTexBuffer()[i2 * 2 + 1];

            // Texcoords if needed (example):
            w[0] = mesh.GetWeiBuffer()[i0];

            w[1] = mesh.GetWeiBuffer()[i1];

            w[2] = mesh.GetWeiBuffer()[i2];

            // Draw the triangle
            draw_triangle(p, t, w, view_matrix, pose_matrix, viewport, in_texture, out_texture);
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
    virtual void vertex_shader(const cpu::Vec3 &inVertex,
                               const cpu::Vec2 &inTexCoord,
                               const float &inWeight,
                               const cpu::Mat4 &view_matrix,
                               const cpu::Mat4 &pose_matrix,
                               cpu::Vec4 &gl_Position,
                               VaryingType &outVarying) = 0;

    virtual void fragment_shader(const cpu::Vec4 &gl_FragCoord,
                                 const VaryingType &inVarying,
                                 const TextureCPU<InTexType> &inTexture,
                                 OutTexType &outFragment) = 0;

    // -------------------------------------------------------------------------
    // draw_triangle: minimal CPU rasterizer for one triangle
    // -------------------------------------------------------------------------
    void draw_triangle(const cpu::Vec3 *verts,
                       const cpu::Vec2 *texcoords,
                       const float *weights,
                       const cpu::Mat4 &view_matrix,
                       const cpu::Mat4 &pose_matrix,
                       const cpu::BoundingBoxType<int> &viewport,
                       const TextureCPU<InTexType> &in_texture,
                       TextureCPU<OutTexType> &out_texture)
    {
        // Step 1: transform each vertex
        VaryingType perVertex[3];
        cpu::Vec4 gl_Position[3];

        for (int i = 0; i < 3; ++i)
        {
            vertex_shader(verts[i], texcoords[i], weights[i], view_matrix, pose_matrix, gl_Position[i], perVertex[i]);

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

            float x_screen = x_ndc * (float)(out_texture.width() - 1);
            float y_screen = y_ndc * (float)(out_texture.height() - 1);

            // Clamp to valid pixel range
            x_screen = std::clamp(x_screen, 0.0f, float(out_texture.width() - 1));
            y_screen = std::clamp(y_screen, 0.0f, float(out_texture.height() - 1));

            // Overwrite gl_Position with final screen coords
            gl_Position[i](0) = x_screen;
            gl_Position[i](1) = y_screen;
        }

        // Step 2: find triangle bounding box in screen space
        cpu::BoundingBoxType<int> tri_bb(cpu::Vec2(gl_Position[0].x(), gl_Position[0].y()),
                                         cpu::Vec2(gl_Position[1].x(), gl_Position[1].y()),
                                         cpu::Vec2(gl_Position[2].x(), gl_Position[2].y()));

        // Intersect with the given viewport
        cpu::BoundingBoxType<int> screen_bb = tri_bb.Intersection(viewport);

        // Step 3: compute barycentric denominator
        float area = triangle_area(
            cpu::Vec2(gl_Position[0](0), gl_Position[0](1)),
            cpu::Vec2(gl_Position[1](0), gl_Position[1](1)),
            cpu::Vec2(gl_Position[2](0), gl_Position[2](1)));

        if (area >= 0.0)
            return;

        float denom = 1.0f / area;

        // Step 4: rasterize each pixel in bounding box
        for (int py = screen_bb.min_y_ - 1; py < screen_bb.max_y_ + 1; ++py)
        {
            for (int px = screen_bb.min_x_ - 1; px < screen_bb.max_x_ + 1; ++px)
            {
                cpu::Vec4 gl_FragCoord;
                gl_FragCoord(0) = float(px);
                gl_FragCoord(1) = float(py);

                // Barycentric coords in 2D
                cpu::Vec3 barycentric = denom * cpu::Vec3(triangle_area(
                                                              cpu::Vec2(gl_FragCoord(0), gl_FragCoord(1)),
                                                              cpu::Vec2(gl_Position[1](0), gl_Position[1](1)),
                                                              cpu::Vec2(gl_Position[2](0), gl_Position[2](1))),
                                                          triangle_area(
                                                              cpu::Vec2(gl_Position[0](0), gl_Position[0](1)),
                                                              cpu::Vec2(gl_FragCoord(0), gl_FragCoord(1)),
                                                              cpu::Vec2(gl_Position[2](0), gl_Position[2](1))),
                                                          triangle_area(
                                                              cpu::Vec2(gl_Position[0](0), gl_Position[0](1)),
                                                              cpu::Vec2(gl_Position[1](0), gl_Position[1](1)),
                                                              cpu::Vec2(gl_FragCoord(0), gl_FragCoord(1))));

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
                cpu::Vec3 perspective = (1.0 / gl_FragCoord(3)) * cpu::Vec3(barycentric(0) * gl_Position[0](3), barycentric(1) * gl_Position[1](3), barycentric(2) * gl_Position[2](3));

                // Interpolate any per-vertex attributes
                VaryingType varying = perspective(0) * perVertex[0] +
                                      perspective(1) * perVertex[1] +
                                      perspective(2) * perVertex[2];

                // Run fragment shader
                OutTexType outColor = out_texture.nodata();
                fragment_shader(gl_FragCoord, varying, in_texture, outColor);

                out_texture.SetTexel(outColor, py, px);
                // Write out to the color attachment
                // rop(out_texture, px, py, outColor);
            }
        }
    }

    InTexType in_nodata_;
    float fx;
    float fy;
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
    void vertex_shader(const cpu::Vec3 &inVertex,
                       const cpu::Vec2 &inTexCoord,
                       const float &inWeight,
                       const cpu::Mat4 &view_matrix,
                       const cpu::Mat4 &pose_matrix,
                       cpu::Vec4 &gl_Position,
                       float &outVarying) override
    {
        gl_Position = (view_matrix * pose_matrix) * cpu::Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        // No attributes in outVarying, so do nothing with it
        outVarying = inVertex(2); // example: store Z in outVarying
    }

    void fragment_shader(const cpu::Vec4 &gl_FragCoord,
                         const float &inVarying,
                         const TextureCPU<float> &inTexture,
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
    : public BaseRendererCPU<cpu::ImageType /*InTexType*/, cpu::Vec2 /*VaryingType*/, cpu::ImageType /*OutTexType*/>
{
public:
    ImageRendererCPU() = default;
    ~ImageRendererCPU() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const cpu::Vec3 &inVertex,
                       const cpu::Vec2 &inTexCoord,
                       const float &inWeight,
                       const cpu::Mat4 &view_matrix,
                       const cpu::Mat4 &pose_matrix,
                       cpu::Vec4 &gl_Position,
                       cpu::Vec2 &outVarying) override
    {
        gl_Position = (view_matrix * pose_matrix) * cpu::Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        // We are using a "float" for VaryingType, so you can store something if needed
        outVarying = inTexCoord; // placeholder
    }

    void fragment_shader(const cpu::Vec4 &gl_FragCoord,
                         const cpu::Vec2 &inVarying,
                         const TextureCPU<cpu::ImageType> &inTexture,
                         cpu::ImageType &outFragment) override
    {
        cpu::ImageType pix = inTexture.Get(inVarying(1), inVarying(0));
        outFragment = pix;

        // Example: color = [checker pattern], ignoring inVarying
        // float fx = std::floor(gl_FragCoord.x() * 0.1f);
        // float fy = std::floor(gl_FragCoord.y() * 0.1f);
        // bool bright = (static_cast<int>(fx + fy) % 2 == 0);

        // For demonstration, store a grayscale in float
        // outFragment = bright ? 1.0f : 0.2f;
    }
};

class DIDxyRendererCPU
    : public BaseRendererCPU<cpu::ImageType /*InTexType*/, cpu::Vec2 /*VaryingType*/, cpu::Vec3 /*OutTexType*/>
{
public:
    DIDxyRendererCPU() = default;
    ~DIDxyRendererCPU() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const cpu::Vec3 &inVertex,
                       const cpu::Vec2 &inTexCoord,
                       const float &inWeight,
                       const cpu::Mat4 &view_matrix,
                       const cpu::Mat4 &pose_matrix,
                       cpu::Vec4 &gl_Position,
                       cpu::Vec2 &outVarying) override
    {
        gl_Position = (view_matrix * pose_matrix) * cpu::Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        outVarying = inTexCoord;
    }

    void fragment_shader(const cpu::Vec4 &gl_FragCoord,
                         const cpu::Vec2 &inVarying,
                         const TextureCPU<cpu::ImageType> &inTexture,
                         cpu::Vec3 &outFragment) override
    {
        // outFragment = inVarying;

        int height = inTexture.height();
        int width = inTexture.width();
        cpu::ImageType nodata = inTexture.nodata();

        int x = int(inVarying(0) * (width - 1));
        int y = int(inVarying(1) * (height - 1));
        int x_p = x + 1;
        int x_m = x - 1;
        int y_p = y + 1;
        int y_m = y - 1;

        if (x_p >= width || x_m < 0 || y_p >= height || y_m < 0)
        {
            // No need to explicitly set to nodata, it is already in the background color
            // outFragment(0) = 0.0f;
            // outFragment(1) = 0.0f;
            // outFragment(2) = 0.0f;
            return;
        }

        float f_y_p = inTexture.GetTexel(y_p, x);
        float f_y_m = inTexture.GetTexel(y_m, x);
        float f_x_p = inTexture.GetTexel(y, x_p);
        float f_x_m = inTexture.GetTexel(y, x_m);

        if (f_x_p == nodata || f_x_m == nodata ||
            f_y_p == nodata || f_y_m == nodata)
        {
            // No need to explicitly set to nodata, it is already in the background color
            // outFragment(0) = 0.0f;
            // outFragment(1) = 0.0f;
            // outFragment(2) = 0.0f;
            return;
        }

        outFragment(0) = (f_x_p - f_x_m) / 2.0f;
        outFragment(1) = (f_y_p - f_y_m) / 2.0f;
        outFragment(2) = 0.0f;
    }
};

class JtraRendererCPU
    : public BaseRendererCPU<cpu::Vec3 /*InTexType*/, cpu::Vec5 /*VaryingType*/, cpu::Vec3 /*OutTexType*/>
{
public:
    JtraRendererCPU() = default;
    ~JtraRendererCPU() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const cpu::Vec3 &inVertex,
                       const cpu::Vec2 &inTexCoord,
                       const float &inWeight,
                       const cpu::Mat4 &view_matrix,
                       const cpu::Mat4 &pose_matrix,
                       cpu::Vec4 &gl_Position,
                       cpu::Vec5 &outVarying) override
    {
        cpu::Vec4 f_ver = pose_matrix * cpu::Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = view_matrix * f_ver;

        outVarying(0) = f_ver(0);
        outVarying(1) = f_ver(1);
        outVarying(2) = f_ver(2);
        outVarying(3) = inTexCoord(0);
        outVarying(4) = inTexCoord(1);
    }

    void fragment_shader(const cpu::Vec4 &gl_FragCoord,
                         const cpu::Vec5 &inVarying,
                         const TextureCPU<cpu::Vec3> &inTexture,
                         cpu::Vec3 &outFragment) override
    {
        // outFragment = inVarying;

        cpu::Vec3 f_ver(inVarying(0), inVarying(1), inVarying(2));
        cpu::Vec3 f_der = inTexture.Get(inVarying(4), inVarying(3));

        float v0 = f_der(0) * fx / f_ver(2);
        float v1 = f_der(1) * fy / f_ver(2);
        float v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        cpu::Vec3 d_f_i_d_tra = cpu::Vec3(v0, v1, v2);
        // cpu::Vec3 d_f_i_d_rot = cpu::Vec3(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        outFragment(0) = d_f_i_d_tra(0);
        outFragment(1) = d_f_i_d_tra(1);
        outFragment(2) = d_f_i_d_tra(2);
        // outFragment(3) = d_f_i_d_rot(0);
        // outFragment(4) = d_f_i_d_rot(1);
        // outFragment(5) = d_f_i_d_rot(2);
    }
};

class JrotRendererCPU
    : public BaseRendererCPU<cpu::Vec3 /*InTexType*/, cpu::Vec5 /*VaryingType*/, cpu::Vec3 /*OutTexType*/>
{
public:
    JrotRendererCPU() = default;
    ~JrotRendererCPU() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const cpu::Vec3 &inVertex,
                       const cpu::Vec2 &inTexCoord,
                       const float &inWeight,
                       const cpu::Mat4 &view_matrix,
                       const cpu::Mat4 &pose_matrix,
                       cpu::Vec4 &gl_Position,
                       cpu::Vec5 &outVarying) override
    {
        cpu::Vec4 f_ver = pose_matrix * cpu::Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = view_matrix * f_ver;

        outVarying(0) = f_ver(0);
        outVarying(1) = f_ver(1);
        outVarying(2) = f_ver(2);
        outVarying(3) = inTexCoord(0);
        outVarying(4) = inTexCoord(1);
    }

    void fragment_shader(const cpu::Vec4 &gl_FragCoord,
                         const cpu::Vec5 &inVarying,
                         const TextureCPU<cpu::Vec3> &inTexture,
                         cpu::Vec3 &outFragment) override
    {
        // outFragment = inVarying;

        cpu::Vec3 f_ver(inVarying(0), inVarying(1), inVarying(2));
        // cpu::Vec2 f_der = inTexture.Get(inVarying(4), inVarying(3));

        cpu::Vec3 v = inTexture.Get(inVarying(4), inVarying(3));

        // float fx = cam_.GetParams()(0);
        // float fy = cam_.GetParams()(1);

        // float v0 = f_der(0) * fx * inTexture.width() / f_ver(2);
        // float v1 = f_der(1) * fy * inTexture.height() / f_ver(2);
        // float v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        // cpu::Vec3 d_f_i_d_tra = cpu::Vec3(v0, v1, v2);
        cpu::Vec3 d_f_i_d_rot = cpu::Vec3(-f_ver(2) * v(1) + f_ver(1) * v(2),
                                          f_ver(2) * v(0) - f_ver(0) * v(2),
                                          -f_ver(1) * v(0) + f_ver(0) * v(1));

        // outFragment(0) = d_f_i_d_tra(0);
        // outFragment(1) = d_f_i_d_tra(1);
        // outFragment(2) = d_f_i_d_tra(2);
        outFragment(0) = d_f_i_d_rot(0);
        outFragment(1) = d_f_i_d_rot(1);
        outFragment(2) = d_f_i_d_rot(2);
    }
};

class JPoseRendererCPU
    : public BaseRendererCPU<cpu::Vec2 /*InTexType*/, cpu::Vec5 /*VaryingType*/, cpu::Vec6 /*OutTexType*/>
{
public:
    JPoseRendererCPU() = default;
    ~JPoseRendererCPU() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const cpu::Vec3 &inVertex,
                       const cpu::Vec2 &inTexCoord,
                       const float &inWeight,
                       const cpu::Mat4 &view_matrix,
                          const cpu::Mat4 &pose_matrix,
                       cpu::Vec4 &gl_Position,
                       cpu::Vec5 &outVarying) override
    {
        cpu::Vec4 f_ver = pose_matrix * cpu::Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = view_matrix * f_ver;
        
        outVarying(0) = f_ver(0);
        outVarying(1) = f_ver(1);
        outVarying(2) = f_ver(2);
        outVarying(3) = inTexCoord(0);
        outVarying(4) = inTexCoord(1);
    }

    void fragment_shader(const cpu::Vec4 &gl_FragCoord,
                         const cpu::Vec5 &inVarying,
                         const TextureCPU<cpu::Vec2> &inTexture,
                         cpu::Vec6 &outFragment) override
    {
        // outFragment = inVarying;

        cpu::Vec3 f_ver(inVarying(0), inVarying(1), inVarying(2));
        cpu::Vec2 f_der = inTexture.Get(inVarying(4), inVarying(3));

        float v0 = f_der(0) * fx * inTexture.width() / f_ver(2);
        float v1 = f_der(1) * fy * inTexture.height() / f_ver(2);
        float v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        cpu::Vec3 d_f_i_d_tra = cpu::Vec3(v0, v1, v2);
        cpu::Vec3 d_f_i_d_rot = cpu::Vec3(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        outFragment(0) = d_f_i_d_tra(0);
        outFragment(1) = d_f_i_d_tra(1);
        outFragment(2) = d_f_i_d_tra(2);
        outFragment(3) = d_f_i_d_rot(0);
        outFragment(4) = d_f_i_d_rot(1);
        outFragment(5) = d_f_i_d_rot(2);
    }
};
