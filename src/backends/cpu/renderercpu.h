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

inline float cross(const Vec2 &a, const Vec2 &b) { return a(0) * b(1) - a(1) * b(0); }
inline float triangle_area(const Vec2 &p0, const Vec2 &p1, const Vec2 &p2) { return cross(p1 - p0, p2 - p0); }

// Edge function E_ab(p) = (yb-ya)*px + (xa-xb)*py + (xb*ya - xa*yb)
inline float edge_func(float ax, float ay, float bx, float by, float px, float py)
{
    return (by - ay) * px + (ax - bx) * py + (bx * ay - ax * by);
}

// Top-left test: returns true if edge is a "top" or "left" edge
inline bool is_top_left(float ax, float ay, float bx, float by)
{
    return (ay == by) ? (bx < ax) : (ay < by);
}

// -----------------------------------------------------------------------------
// BaseRendererCPU (improved)
// -----------------------------------------------------------------------------
template <typename InTexType, typename VaryingType, typename OutTexType>
class BaseRendererCPU
{
public:
    BaseRendererCPU() = default;
    virtual ~BaseRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const SE3 &pose,
                const Camera &cam,
                const TextureCPU<InTexType> &in_texture,
                TextureCPU<OutTexType> &out_texture,
                int in_lvl,
                int out_lvl)
    {
        in_nodata_ = in_texture.nodata();
        out_texture.fill(out_lvl, out_texture.nodata());

        // Matrices
        Mat4 opencv2opengl = Mat4::Identity();
        opencv2opengl(2, 2) = -1.0; // flip Z like your original intent
        Mat4 view_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl;
        Mat4 pose_matrix = pose.matrix();

        fx = cam.GetParams()(0);
        fy = cam.GetParams()(1);

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        const BoundingBoxType<int> viewport(0, W - 1, 0, H - 1);

        // ---- Map mesh buffers (no copies) ----
        auto pos = mesh.MapReadPositions(); // 3 floats/vertex
        auto tex = mesh.MapReadTexcoords(); // 2 floats/vertex
        auto wei = mesh.MapReadWeights();   // 1 float /vertex
        auto idx = mesh.MapReadIndices();   // uint32_t indices

        // Loop over triangles
        for (std::size_t i = 0; i + 2 < idx.size(); i += 3)
        {
            const uint32_t i0 = idx[i + 0];
            const uint32_t i1 = idx[i + 1];
            const uint32_t i2 = idx[i + 2];

            Vec3 v[3];
            Vec2 uv[3];
            float wght[3];

            // gather
            for (int k = 0; k < 3; ++k)
            {
                const uint32_t vi = (k == 0 ? i0 : k == 1 ? i1
                                                          : i2);
                v[k](0) = pos[vi * 3 + 0];
                v[k](1) = pos[vi * 3 + 1];
                v[k](2) = pos[vi * 3 + 2];
                uv[k](0) = tex[vi * 2 + 0];
                uv[k](1) = tex[vi * 2 + 1];
                wght[k] = wei[vi];
            }

            draw_triangle(v, uv, wght, view_matrix, pose_matrix, viewport,
                          in_texture, out_texture, in_lvl, out_lvl);
        }
    }

protected:
    // User-provided shaders
    virtual void vertex_shader(const Vec3 &inVertex,
                               const Vec2 &inTexCoord,
                               const float &inWeight,
                               const Mat4 &view_matrix,
                               const Mat4 &pose_matrix,
                               Vec4 &gl_Position,
                               VaryingType &outVarying) = 0;

    virtual void fragment_shader(const Vec4 &gl_FragCoord,
                                 const VaryingType &inVarying,
                                 const TextureCPU<InTexType> &inTexture,
                                 OutTexType &outFragment,
                                 int in_lvl,
                                 int out_lvl) = 0;

    // Triangle rasterizer (top-left rule, perspective correct)
    void draw_triangle(const Vec3 *verts,
                       const Vec2 *texcoords,
                       const float *weights,
                       const Mat4 &view_matrix,
                       const Mat4 &pose_matrix,
                       const BoundingBoxType<int> &viewport,
                       const TextureCPU<InTexType> &in_texture,
                       TextureCPU<OutTexType> &out_texture,
                       int in_lvl,
                       int out_lvl)
    {
        // Vertex shading & clip → NDC → screen
        struct VSOut
        {
            Vec2 screen;            // x,y in pixel space (float)
            float depth;            // z in [0,1] if your projection is like GL_ZERO_TO_ONE
            float invW;             // 1 / clip.w
            VaryingType var_over_w; // varyings multiplied by invW
            VaryingType var;        // original varyings (for convenience)
        } vout[3];

        for (int i = 0; i < 3; ++i)
        {
            Vec4 clip;
            VaryingType var;
            vertex_shader(verts[i], texcoords[i], weights[i], view_matrix, pose_matrix, clip, var);

            const float invW = 1.0f / clip(3);
            const float ndc_x = clip(0) * invW; // [-1,1]
            const float ndc_y = clip(1) * invW;
            const float ndc_z = clip(2) * invW; // assumed 0..1 after proj (adjust if -1..1)

            // pixel-space (don’t clamp here) — match GL rasterization (remove +1/-0.5 adjustment)
            vout[i].screen(0) = 0.5f * (ndc_x + 1.0f) * (viewport.max_x_ - viewport.min_x_);
            vout[i].screen(1) = 0.5f * (ndc_y + 1.0f) * (viewport.max_y_ - viewport.min_y_);
            vout[i].depth = ndc_z;
            vout[i].invW = invW;
            vout[i].var = var;
            vout[i].var_over_w = var * invW; // requires scalar*VaryingType
        }

        // Back-face cull (optional). Keep CCW (area > 0) – adjust sign to your convention
        const float area = triangle_area(
            Vec2(vout[0].screen(0), vout[0].screen(1)),
            Vec2(vout[1].screen(0), vout[1].screen(1)),
            Vec2(vout[2].screen(0), vout[2].screen(1)));
        if (std::abs(area) < 1e-8f)
            return; // degenerate
        // if (area <= 0) return;            // enable to cull backfaces

        // Triangle bounding box (float → int, clamp to viewport)
        float minx = std::min({vout[0].screen(0), vout[1].screen(0), vout[2].screen(0)});
        float maxx = std::max({vout[0].screen(0), vout[1].screen(0), vout[2].screen(0)});
        float miny = std::min({vout[0].screen(1), vout[1].screen(1), vout[2].screen(1)});
        float maxy = std::max({vout[0].screen(1), vout[1].screen(1), vout[2].screen(1)});

        int x0 = std::max(viewport.min_x_, static_cast<int>(std::floor(minx)));
        int x1 = std::min(viewport.max_x_, static_cast<int>(std::ceil(maxx)));
        int y0 = std::max(viewport.min_y_, static_cast<int>(std::floor(miny)));
        int y1 = std::min(viewport.max_y_, static_cast<int>(std::ceil(maxy)));
        if (x0 > x1 || y0 > y1)
            return;

        // Edge setup (top-left rule)
        const float xA = vout[0].screen(0), yA = vout[0].screen(1);
        const float xB = vout[1].screen(0), yB = vout[1].screen(1);
        const float xC = vout[2].screen(0), yC = vout[2].screen(1);

        const float area2 = edge_func(xA, yA, xB, yB, xC, yC); // 2*area with sign
        if (std::abs(area2) < 1e-8f)
            return; // degenerate

        const float inv_area2 = 1.0f / area2;

        const bool tlAB = is_top_left(xA, yA, xB, yB);
        const bool tlBC = is_top_left(xB, yB, xC, yC);
        const bool tlCA = is_top_left(xC, yC, xA, yA);

        // Evaluate edge functions at top-left corner of each pixel (add +0.5)
        const float px0 = static_cast<float>(x0) + 0.5f;
        const float py0 = static_cast<float>(y0) + 0.5f;

        float eAB_row = edge_func(xA, yA, xB, yB, px0, py0);
        float eBC_row = edge_func(xB, yB, xC, yC, px0, py0);
        float eCA_row = edge_func(xC, yC, xA, yA, px0, py0);

        // Step increments when moving +1 in X or +1 in Y
        const float eAB_dx = (yB - yA);
        const float eAB_dy = (xA - xB);
        const float eBC_dx = (yC - yB);
        const float eBC_dy = (xB - xC);
        const float eCA_dx = (yA - yC);
        const float eCA_dy = (xC - xA);

        // Rasterize
        for (int y = y0; y <= y1; ++y)
        {
            float eAB = eAB_row;
            float eBC = eBC_row;
            float eCA = eCA_row;

            for (int x = x0; x <= x1; ++x)
            {
                // Top-left rule adjustments (include pixels on top/left edges)
                const bool inside =
                    (eAB > 0 || (eAB == 0 && tlAB)) &&
                    (eBC > 0 || (eBC == 0 && tlBC)) &&
                    (eCA > 0 || (eCA == 0 && tlCA));

                if (inside)
                {
                    // Barycentric weights normalized
                    const float w0 = eBC * inv_area2;
                    const float w1 = eCA * inv_area2;
                    const float w2 = eAB * inv_area2;

                    // Perspective: 1/w at pixel
                    const float invW_px = w0 * vout[0].invW + w1 * vout[1].invW + w2 * vout[2].invW;

                    // Interpolate varyings divided by w, then divide by invW_px
                    VaryingType var_over_w_px =
                        w0 * vout[0].var_over_w +
                        w1 * vout[1].var_over_w +
                        w2 * vout[2].var_over_w;
                    VaryingType varying_px = var_over_w_px * (1.0f / invW_px);

                    // Depth (if needed; same trick)
                    float depth_px = w0 * (vout[0].depth * vout[0].invW) +
                                     w1 * (vout[1].depth * vout[1].invW) +
                                     w2 * (vout[2].depth * vout[2].invW);
                    depth_px *= (1.0f / invW_px);
                    // Depth test could go here

                    Vec4 gl_FragCoord;
                    gl_FragCoord(0) = static_cast<float>(x) + 0.5f;
                    gl_FragCoord(1) = static_cast<float>(y) + 0.5f;
                    gl_FragCoord(2) = depth_px;
                    gl_FragCoord(3) = 1.0f / invW_px;

                    OutTexType outColor = out_texture.nodata();
                    fragment_shader(gl_FragCoord, varying_px, in_texture, outColor, in_lvl, out_lvl);
                    out_texture.set_texel_(outColor, y, x, out_lvl);
                }

                // advance to x+1
                eAB += eAB_dx;
                eBC += eBC_dx;
                eCA += eCA_dx;
            }

            // next row y+1: add dy increments and reset x terms
            eAB_row += eAB_dy;
            eBC_row += eBC_dy;
            eCA_row += eCA_dy;
        }
    }

    InTexType in_nodata_{};
    float fx = 0.f;
    float fy = 0.f;
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
    void vertex_shader(const Vec3 &inVertex,
                       const Vec2 &inTexCoord,
                       const float &inWeight,
                       const Mat4 &view_matrix,
                       const Mat4 &pose_matrix,
                       Vec4 &gl_Position,
                       float &outVarying) override
    {
        gl_Position = (view_matrix * pose_matrix) * Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        // No attributes in outVarying, so do nothing with it
        outVarying = inVertex(2); // example: store Z in outVarying
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const float &inVarying,
                         const TextureCPU<float> &inTexture,
                         float &outFragment,
                         int in_lvl,
                         int out_lvl) override
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
    : public BaseRendererCPU<float /*InTexType*/, Vec2 /*VaryingType*/, float /*OutTexType*/>
{
public:
    ImageRendererCPU() = default;
    ~ImageRendererCPU() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec3 &inVertex,
                       const Vec2 &inTexCoord,
                       const float &inWeight,
                       const Mat4 &view_matrix,
                       const Mat4 &pose_matrix,
                       Vec4 &gl_Position,
                       Vec2 &outVarying) override
    {
        gl_Position = (view_matrix * pose_matrix) * Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        // We are using a "float" for VaryingType, so you can store something if needed
        outVarying = inTexCoord; // placeholder
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Vec2 &inVarying,
                         const TextureCPU<float> &inTexture,
                         float &outFragment,
                         int in_lvl,
                         int out_lvl) override
    {
        float pix = inTexture.sample_(inVarying(1), inVarying(0), in_lvl);
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
    : public BaseRendererCPU<float /*InTexType*/, Vec2 /*VaryingType*/, Vec3 /*OutTexType*/>
{
public:
    DIDxyRendererCPU() = default;
    ~DIDxyRendererCPU() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec3 &inVertex,
                       const Vec2 &inTexCoord,
                       const float &inWeight,
                       const Mat4 &view_matrix,
                       const Mat4 &pose_matrix,
                       Vec4 &gl_Position,
                       Vec2 &outVarying) override
    {
        // gl_Position = (view_matrix * pose_matrix) * Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = Vec4(2.0 * inTexCoord(0) - 1.0, 2.0 * inTexCoord(1) - 1.0, 0.0, 1.0);
        outVarying = inTexCoord;
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Vec2 &inVarying,
                         const TextureCPU<float> &inTexture,
                         Vec3 &outFragment,
                         int in_lvl,
                         int out_lvl) override
    {
        // outFragment = inVarying;

        int height = inTexture.height(in_lvl);
        int width = inTexture.width(in_lvl);
        float nodata = inTexture.nodata();

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

        float f = inTexture.texel_(y, x, in_lvl);
        float f_y_p = inTexture.texel_(y_p, x, in_lvl);
        float f_y_m = inTexture.texel_(y_m, x, in_lvl);
        float f_x_p = inTexture.texel_(y, x_p, in_lvl);
        float f_x_m = inTexture.texel_(y, x_m, in_lvl);

        if (f_x_p == nodata || f_x_m == nodata ||
            f_y_p == nodata || f_y_m == nodata || f == nodata)
        {
            // No need to explicitly set to nodata, it is already in the background color
            // outFragment(0) = 0.0f;
            // outFragment(1) = 0.0f;
            // outFragment(2) = 0.0f;
            return;
        }

        outFragment(0) = (f_x_p - f_x_m) / 2.0f;
        outFragment(1) = (f_y_p - f_y_m) / 2.0f;
        outFragment(2) = 0.0; // f; // save the projected frame for later processing
    }
};

class JtraRendererCPU
    : public BaseRendererCPU<Vec3 /*InTexType*/, Vec5 /*VaryingType*/, Vec3 /*OutTexType*/>
{
public:
    JtraRendererCPU() = default;
    ~JtraRendererCPU() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec3 &inVertex,
                       const Vec2 &inTexCoord,
                       const float &inWeight,
                       const Mat4 &view_matrix,
                       const Mat4 &pose_matrix,
                       Vec4 &gl_Position,
                       Vec5 &outVarying) override
    {
        Vec4 f_ver = pose_matrix * Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = view_matrix * f_ver;

        outVarying(0) = f_ver(0);
        outVarying(1) = f_ver(1);
        outVarying(2) = f_ver(2);
        outVarying(3) = inTexCoord(0);
        outVarying(4) = inTexCoord(1);
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Vec5 &inVarying,
                         const TextureCPU<Vec3> &inTexture,
                         Vec3 &outFragment,
                         int in_lvl,
                         int out_lvl) override
    {
        // outFragment = inVarying;

        Vec3 nodata = inTexture.nodata();

        Vec3 f_ver(inVarying(0), inVarying(1), inVarying(2));
        // take the derivative in frame coordinates (not projected)
        // Vec3 f_der = inTexture.sample(inVarying(4), inVarying(3), in_lvl);
        Vec3 f_der = inTexture.texel_(gl_FragCoord(1), gl_FragCoord(0), in_lvl);

        if (f_der == nodata)
            return;

        float v0 = f_der(0) * fx * inTexture.width(in_lvl) / f_ver(2);
        float v1 = f_der(1) * fy * inTexture.height(in_lvl) / f_ver(2);
        float v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        Vec3 d_f_i_d_tra = Vec3(v0, v1, v2);
        // Vec3 d_f_i_d_rot = Vec3(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        outFragment(0) = d_f_i_d_tra(0);
        outFragment(1) = d_f_i_d_tra(1);
        outFragment(2) = d_f_i_d_tra(2);
        // outFragment(3) = d_f_i_d_rot(0);
        // outFragment(4) = d_f_i_d_rot(1);
        // outFragment(5) = d_f_i_d_rot(2);
    }
};

class JrotRendererCPU
    : public BaseRendererCPU<Vec3 /*InTexType*/, Vec5 /*VaryingType*/, Vec3 /*OutTexType*/>
{
public:
    JrotRendererCPU() = default;
    ~JrotRendererCPU() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec3 &inVertex,
                       const Vec2 &inTexCoord,
                       const float &inWeight,
                       const Mat4 &view_matrix,
                       const Mat4 &pose_matrix,
                       Vec4 &gl_Position,
                       Vec5 &outVarying) override
    {
        Vec4 f_ver = pose_matrix * Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = view_matrix * f_ver;

        outVarying(0) = f_ver(0);
        outVarying(1) = f_ver(1);
        outVarying(2) = f_ver(2);
        outVarying(3) = inTexCoord(0);
        outVarying(4) = inTexCoord(1);
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Vec5 &inVarying,
                         const TextureCPU<Vec3> &inTexture,
                         Vec3 &outFragment,
                         int in_lvl,
                         int out_lvl) override
    {
        // outFragment = inVarying;

        Vec3 nodata = inTexture.nodata();

        Vec3 f_ver(inVarying(0), inVarying(1), inVarying(2));
        // Vec2 f_der = inTexture.Get(inVarying(4), inVarying(3));

        // Vec3 v = inTexture.sample_(inVarying(4), inVarying(3), in_lvl);
        Vec3 v = inTexture.texel_(gl_FragCoord(1), gl_FragCoord(0), in_lvl);

        if (v == nodata)
            return;

        // float fx = cam_.GetParams()(0);
        // float fy = cam_.GetParams()(1);

        // float v0 = f_der(0) * fx * inTexture.width() / f_ver(2);
        // float v1 = f_der(1) * fy * inTexture.height() / f_ver(2);
        // float v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        // Vec3 d_f_i_d_tra = Vec3(v0, v1, v2);
        Vec3 d_f_i_d_rot = Vec3(-f_ver(2) * v(1) + f_ver(1) * v(2),
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

class JposeRendererCPU
    : public BaseRendererCPU<Vec3 /*InTexType*/, Vec5 /*VaryingType*/, Vec3 /*OutTexType*/>
{
public:
    JposeRendererCPU() = default;
    ~JposeRendererCPU() override = default;

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const Vec3 &inVertex,
                       const Vec2 &inTexCoord,
                       const float &inWeight,
                       const Mat4 &view_matrix,
                       const Mat4 &pose_matrix,
                       Vec4 &gl_Position,
                       Vec5 &outVarying) override
    {
        Vec4 f_ver = pose_matrix * Vec4(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = view_matrix * f_ver;

        outVarying(0) = f_ver(0);
        outVarying(1) = f_ver(1);
        outVarying(2) = f_ver(2);
        outVarying(3) = inTexCoord(0);
        outVarying(4) = inTexCoord(1);
    }

    void fragment_shader(const Vec4 &gl_FragCoord,
                         const Vec5 &inVarying,
                         const TextureCPU<Vec3> &inTexture,
                         Vec3 &outFragment,
                         int in_lvl,
                         int out_lvl) override
    {
        // outFragment = inVarying;

        Vec3 f_ver(inVarying(0), inVarying(1), inVarying(2));
        Vec3 f_der = inTexture.sample_(inVarying(4), inVarying(3), in_lvl);

        float v0 = f_der(0) * fx * inTexture.width(in_lvl) / f_ver(2);
        float v1 = f_der(1) * fy * inTexture.height(in_lvl) / f_ver(2);
        float v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        Vec3 d_f_i_d_tra = Vec3(v0, v1, v2);
        Vec3 d_f_i_d_rot = Vec3(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        // outFragment(0) = d_f_i_d_tra(0);
        // outFragment(1) = d_f_i_d_tra(1);
        // outFragment(2) = d_f_i_d_tra(2);
        outFragment(0) = d_f_i_d_rot(0);
        outFragment(1) = d_f_i_d_rot(1);
        outFragment(2) = d_f_i_d_rot(2);
    }
};
