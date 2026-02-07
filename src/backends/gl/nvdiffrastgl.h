#pragma once
// #include <string>
// #include <vector>
// #include <utility>
// #include <iostream>
// #include <type_traits>
// #include <cassert>

#include "core/render_constants.h"
#include "core/error_handling.h"
#include "core/types.h"
#include "core/camera.h"
#include "core/boundingbox.h"
#include "backends/gl/devicegl_glad.h"
#include "backends/gl/meshgl.h"
#include "backends/gl/texturegl.h"

static const char *common_nvdiffrast = R"GLSL(
        // Enable one of these depending on your driver.
        // #extension GL_EXT_shader_atomic_float : require
        // #extension GL_NV_shader_atomic_float  : require

        // ---------- SSBO layout ----------
        struct VertexPacked {
            vec4 pos_ws;      // xyz = world position
            vec4 nrm_ws;      // xyz = world normal (assumed fixed for simplicity)
            vec4 uv_ks;       // xy = uv, z = ks (spec strength)
            vec4 shin_pad;    // x = shininess
        };

        layout(std430, binding = 0) readonly buffer Vertices { VertexPacked vtx[]; };
        layout(std430, binding = 1) readonly buffer Triangles { uvec3 tri[]; };

        // Camera parameters and gradients.
        // camRot is axis-angle (small-angle update recommended).
        layout(std430, binding = 2) buffer CameraParams {
            vec4 camPos_ws;   // xyz
            vec4 camRot_aa;   // xyz
            vec4 intr;        // fx, fy, cx, cy (in pixel units)
            vec4 grad_camPos;
            vec4 grad_camRot;
            vec4 grad_intr;
        };

        // Per-vertex output grads.
        layout(std430, binding = 3) buffer VertexGrads {
            vec4 grad_pos_ws[];   // xyz used
            vec2 grad_spec[];     // x: dks, y: dshin
        };

        // ---------- Atomics ----------
        void atomicAddVec3(inout vec4 dst, vec3 v) {
            atomicAdd(dst.x, v.x);
            atomicAdd(dst.y, v.y);
            atomicAdd(dst.z, v.z);
        }

        void atomicAddVec2(inout vec2 dst, vec2 v) {
            atomicAdd(dst.x, v.x);
            atomicAdd(dst.y, v.y);
        }

        // ---------- Math helpers ----------
        vec2 perp(vec2 a) { return vec2(a.y, -a.x); }

        float cross2(vec2 a, vec2 b) { return a.x*b.y - a.y*b.x; }

        struct CamXform {
            mat3 R; // world->cam
        };

        mat3 rodrigues(vec3 w) {
            float th = length(w);
            if (th < 1e-8) return mat3(1.0);
            vec3 k = w / th;
            float c = cos(th), s = sin(th);
            mat3 K = mat3(
                0.0,   -k.z,  k.y,
                k.z,   0.0,  -k.x,
            -k.y,   k.x,  0.0
            );
            return mat3(1.0) + s*K + (1.0 - c)*(K*K);
        }

        CamXform getCam() {
            CamXform c;
            c.R = rodrigues(camRot_aa.xyz);
            return c;
        }

        vec3 worldToCam(vec3 Pw) {
            CamXform c = getCam();
            return c.R * (Pw - camPos_ws.xyz);
        }

        // Project cam-space to pixel coordinates (not NDC).
        vec2 projectPix(vec3 Pc, vec4 intr_, vec2 imgSize) {
            float X = Pc.x, Y = Pc.y, Z = Pc.z;
            float fx = intr_.x, fy = intr_.y, cx = intr_.z, cy = intr_.w;
            // Standard pinhole in pixel coordinates:
            return vec2(fx * (X / Z) + cx, fy * (Y / Z) + cy);
        }

        // Backprop through projection: from g_pix (dL/dx, dL/dy) to dL/dPc and intr grads.
        void backprojPix(
            vec3 Pc, vec2 g_pix,
            inout vec3 g_Pc,
            inout vec4 g_intr_accum
        ){
            float X = Pc.x, Y = Pc.y, Z = Pc.z;
            float fx = intr.x, fy = intr.y;

            // d x / d Pc
            g_Pc.x += g_pix.x * (fx / Z);
            g_Pc.y += g_pix.y * (fy / Z);
            g_Pc.z += g_pix.x * (-fx * X / (Z*Z))
                    + g_pix.y * (-fy * Y / (Z*Z));

            // intrinsics
            g_intr_accum.x += g_pix.x * (X / Z); // dL/dfx
            g_intr_accum.y += g_pix.y * (Y / Z); // dL/dfy
            g_intr_accum.z += g_pix.x;           // dL/dcx
            g_intr_accum.w += g_pix.y;           // dL/dcy
        }

        // Backprop cam-space gradients into world vertex positions + camera extrinsics.
        void backCam(
            vec3 Pc, vec3 g_Pc,
            inout vec3 g_Pw,
            inout vec3 g_camPos,
            inout vec3 g_camRot
        ){
            CamXform c = getCam();
            // Pc = R*(Pw - C)
            // g_Pw = R^T * g_Pc
            g_Pw += transpose(c.R) * g_Pc;

            // g_C = -R^T * g_Pc
            g_camPos += -(transpose(c.R) * g_Pc);

            // small-angle: δPc ≈ δω × Pc  => g_ω += Pc × g_Pc
            g_camRot += cross(Pc, g_Pc);
        }

        // dNormalize: given v and upstream grad g_n where n = normalize(v), return grad wrt v.
        vec3 dNormalize(vec3 v, vec3 g_n) {
            float len = length(v);
            if (len < 1e-12) return vec3(0.0);
            vec3 n = v / len;
            // (I - n n^T)/len * g_n
            return (g_n - n * dot(n, g_n)) / len;
        }
        )GLSL";

class RasterFWRendererGL
{
public:
    RasterFWRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 460 core

            layout(location=0) in vec4 in_pos_clip;

            void main() {
                gl_Position = in_pos_clip;
            }
            )Shader";

        const char *geometry_shader = R"Shader(
            #version 460 core

            layout(triangles) in;
            layout(triangle_strip, max_vertices=3) out;

            layout(location=0) flat out vec4 g_p0;
            layout(location=1) flat out vec4 g_p1;
            layout(location=2) flat out vec4 g_p2;
            layout(location=3) flat out int  g_triId1; // 1-based triangle id (like triIdx+1)
            smooth out vec3  g_bc;           // perspective-correct barycentrics to FS

            uniform int u_triBase;    // add if you do multi-draw / offsets; else 0
            uniform int u_layerBase;  // add if you offset layers; else 0

            void main() {
                vec4 p0 = gl_in[0].gl_Position;
                vec4 p1 = gl_in[1].gl_Position;
                vec4 p2 = gl_in[2].gl_Position;

                int triId1 = gl_PrimitiveIDIn + u_triBase + 1;
                int layer  = gl_InstanceID + u_layerBase;

                for (int i = 0; i < 3; ++i) {
                    g_p0 = p0;
                    g_p1 = p1;
                    g_p2 = p2;
                    g_triId1 = triId1;
                    g_bc     = vec3(i == 0, i == 1, i == 2);

                    gl_Layer    = layer;                 // requires framebuffer with layered attachments
                    gl_Position = gl_in[i].gl_Position;
                    EmitVertex();
                }
                EndPrimitive();
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 460 core

            layout(location=0) flat in vec4 g_p0;
            layout(location=1) flat in vec4 g_p1;
            layout(location=2) flat in vec4 g_p2;
            layout(location=3) flat in int  g_triId1;
            smooth in vec3  g_bc;           // perspective-correct barycentrics to FS

            layout(location=0) out vec4 out_rast; // RGBA32F
            layout(location=1) out vec4 out_db;   // RGBA32F

            uniform ivec2 u_resolution; // (width, height)

            float triidx_to_float(int triId1) {
                return uintBitsToFloat(uint(triId1)); // matches CUDA-style bit packing
            }

            void main() {
                int W = u_resolution.x;
                int H = u_resolution.y;

                // Pixel indices (OpenGL window coords are bottom-left origin by default).
                int px = int(gl_FragCoord.x); // gl_FragCoord is at (i+0.5, j+0.5)
                int py = int(gl_FragCoord.y);

                // Map pixel center -> NDC like CUDA's fx = xs*px + xo (with +0.5 baked in).
                float fx = (2.0 * (float(px) + 0.5) / float(W)) - 1.0;
                float fy = (2.0 * (float(py) + 0.5) / float(H)) - 1.0;

                vec4 p0 = g_p0;
                vec4 p1 = g_p1;
                vec4 p2 = g_p2;

                // Evaluate edge functions in clip space (same as CUDA).
                float p0x = p0.x - fx * p0.w;
                float p0y = p0.y - fy * p0.w;
                float p1x = p1.x - fx * p1.w;
                float p1y = p1.y - fy * p1.w;
                float p2x = p2.x - fx * p2.w;
                float p2y = p2.y - fy * p2.w;

                float a0 = p1x * p2y - p1y * p2x;
                float a1 = p2x * p0y - p2y * p0x;
                float a2 = p0x * p1y - p0y * p1x;

                float iw = 1.0 / (a0 + a1 + a2);
                float b0 = a0 * iw;
                float b1 = a1 * iw;

                // z/w depth (NDC z)
                float z  = p0.z * a0 + p1.z * a1 + p2.z * a2;
                float w  = p0.w * a0 + p1.w * a1 + p2.w * a2;
                float zw = z / w;

                // Clamps to avoid NaNs (as in CUDA)
                b0 = clamp(b0, 0.0, 1.0);
                b1 = clamp(b1, 0.0, 1.0);
                float bs = 1.0 / max(b0 + b1, 1.0);
                b0 *= bs;
                b1 *= bs;
                zw = clamp(zw, -1.0, 1.0);

                out_rast = vec4(b0, b1, zw, triidx_to_float(g_triId1));

                // Bary pixel differentials (same math as CUDA)
                float xs = 2.0 / float(W);
                float ys = 2.0 / float(H);

                float dfxdx = xs * iw;
                float dfydy = ys * iw;

                float da0dx = p2.y * p1.w - p1.y * p2.w;
                float da0dy = p1.x * p2.w - p2.x * p1.w;
                float da1dx = p0.y * p2.w - p2.y * p0.w;
                float da1dy = p2.x * p0.w - p0.x * p2.w;
                float da2dx = p1.y * p0.w - p0.y * p1.w;
                float da2dy = p0.x * p1.w - p1.x * p0.w;

                float datdx = da0dx + da1dx + da2dx;
                float datdy = da0dy + da1dy + da2dy;

                float dudx = dfxdx * (b0 * datdx - da0dx);
                float dudy = dfydy * (b0 * datdy - da0dy);
                float dvdx = dfxdx * (b1 * datdx - da1dx);
                float dvdy = dfydy * (b1 * datdy - da1dy);

                out_db = vec4(dudx, dudy, dvdx, dvdy);
            }
            )Shader";

        create_framebuffer(fbo_, rbo_);
        program_ = create_program(vertex_shader, geometry_shader, fragment_shader);
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int out_lvl,
                TextureGL<Vec4<float>> &rast_texture)
    {
        // save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rast_texture.id(), out_lvl);

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(rast_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(rast_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        Vec4<float> rast_nodata = rast_texture.nodata();

        float rast_clear[4] = {rast_nodata(0), rast_nodata(1), rast_nodata(2), rast_nodata(3)};

        // Clear GL_COLOR_ATTACHMENT0
        const GLenum bufs1[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs1);
        glClearColor(rast_clear[0], rast_clear[1], rast_clear[2], rast_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Restore glDrawBuffers for subsequent rendering.
        // This assumes the original setup was GL_COLOR_ATTACHMENT0 and GL_COLOR_ATTACHMENT1
        // as done in the clear_buffers function.
        glDrawBuffers(1, bufs);
        // #endif

        glUseProgram(program_);

        const Mat4<float> view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE,
                                                                RenderConstants::FAR_PLANE);
        const Mat4<float> pose_matrix = pose.matrix();

        glUniformMatrix4fv(glGetUniformLocation(program_, "view_matrix"), 1, GL_FALSE, view_matrix.data());
        glUniformMatrix4fv(glGetUniformLocation(program_, "pose_matrix"), 1, GL_FALSE, pose_matrix.data());

        mesh.draw();

        glUseProgram(program_);

        // restore_state();
    }

private:
    GLuint fbo_;
    GLuint rbo_;
    GLuint program_;
};

class RasterBWRendererGL
{
public:
    RasterBWRendererGL()
    {
        const char *compute_shader = R"Shader(
        #version 460 core
        #extension GL_ARB_shader_atomic_float : require

        layout(local_size_x=16, local_size_y=16, local_size_z=1) in;

        layout(rgba32f, binding=0) uniform readonly image2DArray u_rast; // (b0,b1,zw,triBits)
        layout(rg32f,   binding=1) uniform readonly image2DArray u_dy;   // (dy0,dy1)

        layout(std430, binding=0) readonly buffer PosBuf { vec4 pos[]; };
        layout(std430, binding=1) readonly buffer TriBuf { int  tri[]; };

        layout(std430, binding=2) buffer GradBuf { float grad[]; };

        uniform int   u_numTriangles;
        uniform int   u_numVertices;     // vertices per instance (same as your forward)
        uniform bool  u_instanceMode;
        uniform ivec2 u_resolution;      // (W,H)

        int float_to_triidx(float fBitsAsFloat) {
            return int(floatBitsToUint(fBitsAsFloat)); // 1-based; 0 means empty
        }

        void atomicAdd_xyw(int vi, float gx, float gy, float gw) {
            int base = 4 * vi;
            atomicAdd(grad[base + 0], gx);
            atomicAdd(grad[base + 1], gy);
            atomicAdd(grad[base + 3], gw);
        }

        void main() {
            int px = int(gl_GlobalInvocationID.x);
            int py = int(gl_GlobalInvocationID.y);
            int pz = int(gl_GlobalInvocationID.z);

            int W = u_resolution.x;
            int H = u_resolution.y;
            if (px >= W || py >= H) return;

            vec4 rast = imageLoad(u_rast, ivec3(px, py, pz));
            vec2 dy   = imageLoad(u_dy,   ivec3(px, py, pz)).xy;

            if (dy.x == 0.0 && dy.y == 0.0) return;

            int triId1 = float_to_triidx(rast.w);
            int triIdx = triId1 - 1;
            if (triIdx < 0 || triIdx >= u_numTriangles) return;

            int vi0 = tri[triIdx * 3 + 0];
            int vi1 = tri[triIdx * 3 + 1];
            int vi2 = tri[triIdx * 3 + 2];

            if (vi0 < 0 || vi0 >= u_numVertices ||
                vi1 < 0 || vi1 >= u_numVertices ||
                vi2 < 0 || vi2 >= u_numVertices) return;

            if (u_instanceMode) {
                vi0 += pz * u_numVertices;
                vi1 += pz * u_numVertices;
                vi2 += pz * u_numVertices;
            }

            vec4 p0 = pos[vi0];
            vec4 p1 = pos[vi1];
            vec4 p2 = pos[vi2];

            // Pixel center -> clip coords (matches xs/xo mapping)
            float fx = (2.0 * (float(px) + 0.5) / float(W)) - 1.0;
            float fy = (2.0 * (float(py) + 0.5) / float(H)) - 1.0;

            float p0x = p0.x - fx * p0.w;
            float p0y = p0.y - fy * p0.w;
            float p1x = p1.x - fx * p1.w;
            float p1y = p1.y - fy * p1.w;
            float p2x = p2.x - fx * p2.w;
            float p2y = p2.y - fy * p2.w;

            float a0 = p1x*p2y - p1y*p2x;
            float a1 = p2x*p0y - p2y*p0x;
            float a2 = p0x*p1y - p0y*p1x;
            float at = a0 + a1 + a2;

            float ep = (at >= 0.0) ? 1e-6 : -1e-6;
            float iw = 1.0 / (at + ep);

            float b0 = a0 * iw;
            float b1 = a1 * iw;

            float gb0  = dy.x * iw;
            float gb1  = dy.y * iw;
            float gbb  = gb0 * b0 + gb1 * b1;

            float gp0x = gbb * (p2y - p1y) - gb1 * p2y;
            float gp1x = gbb * (p0y - p2y) + gb0 * p2y;
            float gp2x = gbb * (p1y - p0y) - gb0 * p1y + gb1 * p0y;

            float gp0y = gbb * (p1x - p2x) + gb1 * p2x;
            float gp1y = gbb * (p2x - p0x) - gb0 * p2x;
            float gp2y = gbb * (p0x - p1x) + gb0 * p1x - gb1 * p0x;

            float gp0w = -fx * gp0x - fy * gp0y;
            float gp1w = -fx * gp1x - fy * gp1y;
            float gp2w = -fx * gp2x - fy * gp2y;

            atomicAdd_xyw(vi0, gp0x, gp0y, gp0w);
            atomicAdd_xyw(vi1, gp1x, gp1y, gp1w);
            atomicAdd_xyw(vi2, gp2x, gp2y, gp2w);
        }
        )Shader";

        program_ = create_program(compute_shader);
    }

    void Render(int lvl,
                const TextureGL<Vec4<float>> &rast_texture,
                const TextureGL<Vec2<float>> &dy_texture,
                const BufferGL<float> &pos_buffer,
                const BufferGL<int> &tri_buffer,
                BufferGL<float> &grad_buffer,
                int numTriangles,
                int numVerticesPerInstance,
                bool instanceMode,
                int depthLayers) // number of layers in the 2DArray (pz)
    {
        const int width = rast_texture.width(lvl);
        const int height = rast_texture.height(lvl);

        const int groupsX = (width + 15) / 16;
        const int groupsY = (height + 15) / 16;

        // Bind SSBOs WITHOUT reallocating them.
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pos_buffer.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, tri_buffer.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, grad_buffer.id());

        // Bind images with correct formats.
        // Use layered = GL_TRUE so the shader can index by pz.
        glBindImageTexture(0, rast_texture.id(), lvl, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
        glBindImageTexture(1, dy_texture.id(), lvl, GL_TRUE, 0, GL_READ_ONLY, GL_RG32F);

        glUseProgram(program_);

        glUniform1i(glGetUniformLocation(program_, "u_numTriangles"), numTriangles);
        glUniform1i(glGetUniformLocation(program_, "u_numVertices"), numVerticesPerInstance);
        glUniform1i(glGetUniformLocation(program_, "u_instanceMode"), instanceMode ? 1 : 0);
        glUniform2i(glGetUniformLocation(program_, "u_resolution"), width, height);

        // Dispatch across layers so pz is valid.
        glDispatchCompute(groupsX, groupsY, depthLayers);

        // Make SSBO writes visible to subsequent passes / readbacks.
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

private:
    GLuint program_ = 0;
};
/*
class InterpolationFWRendererGL
{
public:
    InterpolationFWRendererGL()
    {
        const char *compute_shader = R"Shader(
            #version 460 core

            // Toggle at compile time:
            //   -DENABLE_DA=0   or   -DENABLE_DA=1
            #ifndef ENABLE_DA
            #define ENABLE_DA 0
            #endif

            layout(local_size_x=16, local_size_y=16, local_size_z=1) in;

            // rast: (b0,b1,zw,triIdBits-as-float)
            layout(rgba32f, binding=0) uniform readonly image2DArray u_rast;

            #if ENABLE_DA
            // rastDB: (dudx,dudy,dvdx,dvdy)
            layout(rgba32f, binding=1) uniform readonly image2DArray u_rastDB;
            #endif

            // Geometry
            layout(std430, binding=0) readonly buffer TriBuf { int tri[]; };     // numTriangles*3
            layout(std430, binding=1) readonly buffer AttrBuf { float attr[]; }; // numVerticesTotal*numAttr

            // Outputs
            layout(std430, binding=2) buffer OutBuf { float outAttr[]; };        // (W*H*D)*numAttr
            #if ENABLE_DA
            layout(std430, binding=3) buffer OutDABuf { vec2 outDA[]; };         // (W*H*D)*numDiffAttr
            layout(std430, binding=4) readonly buffer DiffIdxBuf { int diffIdx[]; }; // numDiffAttr (if !diff_attrs_all)
            #endif

            uniform ivec2 u_resolution;     // (W,H)
            uniform int   u_depth;          // D
            uniform int   u_numTriangles;
            uniform int   u_numVertices;    // per instance
            uniform int   u_numAttr;
            uniform int   u_numDiffAttr;

            uniform bool  u_instanceMode;   // p.instance_mode
            uniform bool  u_attrBC;         // p.attrBC (broadcast attrs across batch)
            #if ENABLE_DA
            uniform bool  u_diff_attrs_all; // p.diff_attrs_all
            #endif

            int float_to_triidx(float fBits) { return int(floatBitsToUint(fBits)); } // 1-based; 0 => none

            void main() {
                int px = int(gl_GlobalInvocationID.x);
                int py = int(gl_GlobalInvocationID.y);
                int pz = int(gl_GlobalInvocationID.z);

                int W = u_resolution.x;
                int H = u_resolution.y;
                if (px >= W || py >= H || pz >= u_depth) return;

                int pidx = px + W * (py + H * pz);

                // Read rast
                vec4 r = imageLoad(u_rast, ivec3(px, py, pz));
                int triIdx = float_to_triidx(r.w) - 1;
                bool triValid = (triIdx >= 0 && triIdx < u_numTriangles);

                // If invalid: write zeros like CUDA (warp-all optimization omitted)
                if (!triValid) {
                    int outBase = pidx * u_numAttr;
                    for (int i = 0; i < u_numAttr; ++i) outAttr[outBase + i] = 0.0;

                #if ENABLE_DA
                    int daBase = pidx * u_numDiffAttr;
                    for (int i = 0; i < u_numDiffAttr; ++i) outDA[daBase + i] = vec2(0.0);
                #endif
                    return;
                }

                // Vertex indices
                int vi0 = tri[triIdx * 3 + 0];
                int vi1 = tri[triIdx * 3 + 1];
                int vi2 = tri[triIdx * 3 + 2];

                if (vi0 < 0 || vi0 >= u_numVertices ||
                    vi1 < 0 || vi1 >= u_numVertices ||
                    vi2 < 0 || vi2 >= u_numVertices) {
                    return;
                }

                // Instance mode: adjust unless broadcasting attributes
                if (u_instanceMode && !u_attrBC) {
                    int off = pz * u_numVertices;
                    vi0 += off; vi1 += off; vi2 += off;
                }

                int a0Base = vi0 * u_numAttr;
                int a1Base = vi1 * u_numAttr;
                int a2Base = vi2 * u_numAttr;

                // Barycentrics (if triValid)
                float b0 = r.x;
                float b1 = r.y;
                float b2 = 1.0 - r.x - r.y;

                // Interpolate attributes
                int outBase = pidx * u_numAttr;
                for (int i = 0; i < u_numAttr; ++i) {
                    float s0 = attr[a0Base + i];
                    float s1 = attr[a1Base + i];
                    float s2 = attr[a2Base + i];
                    outAttr[outBase + i] = b0*s0 + b1*s1 + b2*s2;
                }

            #if ENABLE_DA
                // Pixel differentials of chosen attributes
                vec4 db = imageLoad(u_rastDB, ivec3(px, py, pz));
                float dudx = db.x, dudy = db.y, dvdx = db.z, dvdy = db.w;

                int daBase = pidx * u_numDiffAttr;
                for (int i = 0; i < u_numDiffAttr; ++i) {
                    int j = u_diff_attrs_all ? i : diffIdx[i];
                    if (j < 0) j += u_numAttr; // Python-style negative indices

                    float dsdx = 0.0, dsdy = 0.0;
                    if (j >= 0 && j < u_numAttr) {
                        float s0 = attr[a0Base + j];
                        float s1 = attr[a1Base + j];
                        float s2 = attr[a2Base + j];
                        float dsdu = s0 - s2;
                        float dsdv = s1 - s2;
                        dsdx = dudx*dsdu + dvdx*dsdv;
                        dsdy = dudy*dsdu + dvdy*dsdv;
                    }
                    outDA[daBase + i] = vec2(dsdx, dsdy);
                }
            #endif
            }
            )Shader";

        program_ = create_program(compute_shader);
    }

    void Render(const MeshGL &mesh,
                int out_lvl,
                const TextureGL<Vec4<float>> &rast_texture,
                const TextureGL<Vec2<float>> &dy_texture,
                const BufferGL<float> pos_buffer,
                const BufferGL<int> tri_buffer,
                BufferGL<float> grad_buffer)
    {
        int width = _texture.width(lvl);
        int height = image_1_texture.height(lvl);

        int groupsX = (width + 15) / 16;
        int groupsY = (height + 15) / 16;
        int numGroups = groupsX * groupsY;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, error_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numGroups * sizeof(float), nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, error_ssbo_);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numGroups * sizeof(int), nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, count_ssbo_);

        float image_1_nodata = image_1_texture.nodata();
        float image_2_nodata = image_2_texture.nodata();

        glBindImageTexture(0, image_1_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
        glBindImageTexture(1, image_2_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

        glUseProgram(computeProgram_);
        glUniform2i(glGetUniformLocation(computeProgram_, "uSize"), width, height);
        glUniform1i(glGetUniformLocation(computeProgram_, "uNumGroupsX"), groupsX);

        glDispatchCompute(groupsX, groupsY, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, error_ssbo_);
        float *partial_sum = (float *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                                       numGroups * sizeof(float),
                                                       GL_MAP_READ_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_ssbo_);
        int *partial_count = (int *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                                     numGroups * sizeof(int),
                                                     GL_MAP_READ_BIT);

        double sum = 0.0;
        int total_count = 0;
        for (int i = 0; i < numGroups; i++)
        {
            sum += (double)partial_sum[i];
            total_count += partial_count[i];
        }

        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        error = std::sqrt(sum / total_count);
    }

private:
    GLuint program_;
};

class InterpolationBWRendererGL
{
public:
    InterpolationBWRendererGL()
    {
        const char *compute_shader = R"Shader(
            #version 460 core
            #extension GL_ARB_shader_atomic_float : require

            #ifndef ENABLE_DA
            #define ENABLE_DA 0
            #endif

            layout(local_size_x=16, local_size_y=16, local_size_z=1) in;

            // Inputs
            layout(rgba32f, binding=0) uniform readonly image2DArray u_rast;     // (b0,b1,zw,triIdBits)
            layout(rgba32f, binding=1) uniform readonly image2DArray u_rastDB;   // (dudx,dudy,dvdx,dvdy) (used only if ENABLE_DA)

            // Outputs (like CUDA)
            layout(rgba32f, binding=2) uniform writeonly image2DArray u_gradRaster;    // (gb0,gb1,0,0)
            layout(rgba32f, binding=3) uniform writeonly image2DArray u_gradRasterDB;  // (gdudx,gdudy,gdvdx,gdvdy) if ENABLE_DA

            // Geometry + attributes
            layout(std430, binding=0) readonly buffer TriBuf  { int   tri[]; };
            layout(std430, binding=1) readonly buffer AttrBuf { float attr[]; };

            // Incoming grads
            layout(std430, binding=2) readonly buffer DyBuf   { float dy[]; }; // (W*H*D)*numAttr
            #if ENABLE_DA
            layout(std430, binding=3) readonly buffer DdaBuf  { vec2  dda[]; }; // (W*H*D)*numDiffAttr
            layout(std430, binding=4) readonly buffer DiffIdxBuf { int diffIdx[]; };
            #endif

            // Accumulated output grads
            layout(std430, binding=5) buffer GradAttrBuf { float gradAttr[]; }; // same shape as attr

            uniform ivec2 u_resolution;   // (W,H)
            uniform int   u_depth;
            uniform int   u_numTriangles;
            uniform int   u_numVertices;  // per instance
            uniform int   u_numAttr;
            uniform int   u_numDiffAttr;

            uniform bool  u_instanceMode;
            uniform bool  u_attrBC;
            #if ENABLE_DA
            uniform bool  u_diff_attrs_all;
            #endif

            int float_to_triidx(float fBits) { return int(floatBitsToUint(fBits)); }

            void atomicAddAttr(int vi, int i, float v) {
                atomicAdd(gradAttr[vi * u_numAttr + i], v);
            }

            void main() {
                int px = int(gl_GlobalInvocationID.x);
                int py = int(gl_GlobalInvocationID.y);
                int pz = int(gl_GlobalInvocationID.z);

                int W = u_resolution.x;
                int H = u_resolution.y;
                if (px >= W || py >= H || pz >= u_depth) return;

                int pidx = px + W * (py + H * pz);

                vec4 r = imageLoad(u_rast, ivec3(px, py, pz));
                int triIdx = float_to_triidx(r.w) - 1;

                // If no triangle: write zero grads and exit (matches CUDA)
                if (triIdx < 0 || triIdx >= u_numTriangles) {
                    imageStore(u_gradRaster,   ivec3(px, py, pz), vec4(0.0));
                #if ENABLE_DA
                    imageStore(u_gradRasterDB, ivec3(px, py, pz), vec4(0.0));
                #endif
                    return;
                }

                int vi0 = tri[triIdx * 3 + 0];
                int vi1 = tri[triIdx * 3 + 1];
                int vi2 = tri[triIdx * 3 + 2];

                if (vi0 < 0 || vi0 >= u_numVertices ||
                    vi1 < 0 || vi1 >= u_numVertices ||
                    vi2 < 0 || vi2 >= u_numVertices) {
                    return;
                }

                if (u_instanceMode && !u_attrBC) {
                    int off = pz * u_numVertices;
                    vi0 += off; vi1 += off; vi2 += off;
                }

                float b0 = r.x;
                float b1 = r.y;
                float b2 = 1.0 - r.x - r.y;

                int a0Base = vi0 * u_numAttr;
                int a1Base = vi1 * u_numAttr;
                int a2Base = vi2 * u_numAttr;

                int dyBase = pidx * u_numAttr;

                // Accumulate bary gradients
                float gb0 = 0.0;
                float gb1 = 0.0;

                // Main attribute loop (matches CUDA)
                for (int i = 0; i < u_numAttr; ++i) {
                    float y  = dy[dyBase + i];
                    float s0 = attr[a0Base + i];
                    float s1 = attr[a1Base + i];
                    float s2 = attr[a2Base + i];

                    gb0 += y * (s0 - s2);
                    gb1 += y * (s1 - s2);

                    atomicAddAttr(vi0, i, b0 * y);
                    atomicAddAttr(vi1, i, b1 * y);
                    atomicAddAttr(vi2, i, b2 * y);
                }

                imageStore(u_gradRaster, ivec3(px, py, pz), vec4(gb0, gb1, 0.0, 0.0));

            #if ENABLE_DA
                // Pixel differential gradients
                vec4 db = imageLoad(u_rastDB, ivec3(px, py, pz));
                float dudx = db.x, dudy = db.y, dvdx = db.z, dvdy = db.w;

                float gdudx = 0.0, gdudy = 0.0, gdvdx = 0.0, gdvdy = 0.0;

                int ddaBase = pidx * u_numDiffAttr;

                for (int i = 0; i < u_numDiffAttr; ++i) {
                    int j = u_diff_attrs_all ? i : diffIdx[i];
                    if (j < 0) j += u_numAttr;

                    if (j >= 0 && j < u_numAttr) {
                        vec2 g = dda[ddaBase + i];
                        float dsdx = g.x;
                        float dsdy = g.y;

                        float s0 = attr[a0Base + j];
                        float s1 = attr[a1Base + j];
                        float s2 = attr[a2Base + j];

                        // Gradients wrt db
                        float dsdu = s0 - s2;
                        float dsdv = s1 - s2;

                        gdudx += dsdu * dsdx;
                        gdudy += dsdu * dsdy;
                        gdvdx += dsdv * dsdx;
                        gdvdy += dsdv * dsdy;

                        // Gradients wrt attributes
                        float du = dsdx*dudx + dsdy*dudy;
                        float dv = dsdx*dvdx + dsdy*dvdy;

                        atomicAddAttr(vi0, j,  du);
                        atomicAddAttr(vi1, j,  dv);
                        atomicAddAttr(vi2, j, -du - dv);
                    }
                }

                imageStore(u_gradRasterDB, ivec3(px, py, pz), vec4(gdudx, gdudy, gdvdx, gdvdy));
            #endif
            }
            )Shader";

        program_ = create_program(compute_shader);
    }

    void Render(const MeshGL &mesh,
                int out_lvl,
                const TextureGL<Vec4<float>> &rast_texture,
                const TextureGL<Vec2<float>> &dy_texture,
                const BufferGL<float> pos_buffer,
                const BufferGL<int> tri_buffer,
                BufferGL<float> grad_buffer)
    {
        int width = image_1_texture.width(lvl);
        int height = image_1_texture.height(lvl);

        int groupsX = (width + 15) / 16;
        int groupsY = (height + 15) / 16;
        int numGroups = groupsX * groupsY;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, error_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numGroups * sizeof(float), nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, error_ssbo_);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numGroups * sizeof(int), nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, count_ssbo_);

        float image_1_nodata = image_1_texture.nodata();
        float image_2_nodata = image_2_texture.nodata();

        glBindImageTexture(0, image_1_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
        glBindImageTexture(1, image_2_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

        glUseProgram(computeProgram_);
        glUniform2i(glGetUniformLocation(computeProgram_, "uSize"), width, height);
        glUniform1i(glGetUniformLocation(computeProgram_, "uNumGroupsX"), groupsX);

        glDispatchCompute(groupsX, groupsY, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, error_ssbo_);
        float *partial_sum = (float *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                                       numGroups * sizeof(float),
                                                       GL_MAP_READ_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_ssbo_);
        int *partial_count = (int *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                                     numGroups * sizeof(int),
                                                     GL_MAP_READ_BIT);

        double sum = 0.0;
        int total_count = 0;
        for (int i = 0; i < numGroups; i++)
        {
            sum += (double)partial_sum[i];
            total_count += partial_count[i];
        }

        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        error = std::sqrt(sum / total_count);
    }

private:
    GLuint program_;
};

class TextureFWRendererGL
{
public:
    TextureFWRendererGL()
    {
        const char *compute_shader = R"Shader(
            #version 460 core

            layout(local_size_x=16, local_size_y=16, local_size_z=1) in;

            layout(std430, binding=0) readonly buffer TexBuf { vec4 tex[]; };

            // Per-pixel UVs (and derivatives)
            layout(std430, binding=1) readonly buffer UvBuf { vec2 uv[]; };
            layout(std430, binding=2) readonly buffer UvDaBuf { vec4 uv_da[]; }; // (dudx,dudy,dvdx,dvdy), optional

            layout(std430, binding=3) writeonly buffer OutBuf { vec4 outColor[]; };

            uniform ivec2 u_resolution;
            uniform int   u_depth;

            uniform int   u_mipLevelMax;
            uniform int   u_boundaryMode;     // wrap/clamp/zero
            uniform bool  u_enableMip;
            uniform bool  u_enableTrilinear;

            uniform ivec2 u_mipSize[16];      // sizes per level (set enough for your max)
            uniform int   u_mipOffset[16];    // offsets per level (in texels)

            float computeLod(vec2 dUVdx, vec2 dUVdy, ivec2 size) {
                vec2 dx = dUVdx * vec2(size);
                vec2 dy = dUVdy * vec2(size);
                float rho = max(length(dx), length(dy));
                return log2(max(rho, 1e-8));
            }

            struct BilinearTapInfo {
                ivec2 p00, p10, p01, p11;
                vec2  f;
                bool  ok00, ok10, ok01, ok11;
            };

            bool applyBoundary(inout ivec2 xy, ivec2 size, int boundaryMode) {
                if (boundaryMode == 0) { // wrap
                    xy = ivec2((xy.x % size.x + size.x) % size.x,
                            (xy.y % size.y + size.y) % size.y);
                    return true;
                }
                if (boundaryMode == 1) { // clamp
                    xy = clamp(xy, ivec2(0), size - ivec2(1));
                    return true;
                }
                return (xy.x >= 0 && xy.y >= 0 && xy.x < size.x && xy.y < size.y); // zero
            }

            int idxTexel(int offsetTexels, ivec2 size, int layer, ivec2 xy) {
                return offsetTexels + (layer * size.x * size.y) + (xy.x + size.x * xy.y);
            }

            vec4 sampleBilinearLevel(int level, int layer, vec2 uv, out BilinearTapInfo info) {
                ivec2 size = u_mipSize[level];
                int base   = u_mipOffset[level];

                vec2 p = uv * vec2(size) - vec2(0.5);
                ivec2 i0 = ivec2(floor(p));
                vec2  f  = fract(p);

                info.f = f;
                info.p00 = i0 + ivec2(0,0);
                info.p10 = i0 + ivec2(1,0);
                info.p01 = i0 + ivec2(0,1);
                info.p11 = i0 + ivec2(1,1);

                ivec2 q00 = info.p00, q10 = info.p10, q01 = info.p01, q11 = info.p11;
                info.ok00 = applyBoundary(q00, size, u_boundaryMode);
                info.ok10 = applyBoundary(q10, size, u_boundaryMode);
                info.ok01 = applyBoundary(q01, size, u_boundaryMode);
                info.ok11 = applyBoundary(q11, size, u_boundaryMode);

                vec4 c00 = info.ok00 ? tex[idxTexel(base, size, layer, q00)] : vec4(0.0);
                vec4 c10 = info.ok10 ? tex[idxTexel(base, size, layer, q10)] : vec4(0.0);
                vec4 c01 = info.ok01 ? tex[idxTexel(base, size, layer, q01)] : vec4(0.0);
                vec4 c11 = info.ok11 ? tex[idxTexel(base, size, layer, q11)] : vec4(0.0);

                vec4 cx0 = mix(c00, c10, f.x);
                vec4 cx1 = mix(c01, c11, f.x);
                return mix(cx0, cx1, f.y);
            }

            void main() {
                int px = int(gl_GlobalInvocationID.x);
                int py = int(gl_GlobalInvocationID.y);
                int pz = int(gl_GlobalInvocationID.z);

                int W = u_resolution.x, H = u_resolution.y;
                if (px >= W || py >= H || pz >= u_depth) return;

                int pidx = px + W * (py + H * pz);
                vec2 UV  = uv[pidx];

                int level0 = 0, level1 = 0;
                float t = 0.0;

                if (u_enableMip) {
                    vec4 da = uv_da[pidx];
                    vec2 dUVdx = vec2(da.x, da.z);
                    vec2 dUVdy = vec2(da.y, da.w);

                    float lod = computeLod(dUVdx, dUVdy, u_mipSize[0]);
                    lod = clamp(lod, 0.0, float(u_mipLevelMax));

                    if (u_enableTrilinear) {
                        float fl = floor(lod);
                        level0 = int(fl);
                        level1 = min(level0 + 1, u_mipLevelMax);
                        t = lod - fl;
                    } else {
                        level0 = int(round(lod));
                        level1 = level0;
                        t = 0.0;
                    }
                }

                BilinearTapInfo info0, info1;
                vec4 c0 = sampleBilinearLevel(level0, pz, UV, info0);
                if (level1 == level0) {
                    outColor[pidx] = c0;
                } else {
                    vec4 c1 = sampleBilinearLevel(level1, pz, UV, info1);
                    outColor[pidx] = mix(c0, c1, t);
                }
            }
            )Shader";

        program_ = create_program(compute_shader);
    }

    void Render(const MeshGL &mesh,
                int out_lvl,
                const TextureGL<Vec4<float>> &rast_texture,
                const TextureGL<Vec2<float>> &dy_texture,
                const BufferGL<float> pos_buffer,
                const BufferGL<int> tri_buffer,
                BufferGL<float> grad_buffer)
    {
        int width = image_1_texture.width(lvl);
        int height = image_1_texture.height(lvl);

        int groupsX = (width + 15) / 16;
        int groupsY = (height + 15) / 16;
        int numGroups = groupsX * groupsY;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, error_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numGroups * sizeof(float), nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, error_ssbo_);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numGroups * sizeof(int), nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, count_ssbo_);

        float image_1_nodata = image_1_texture.nodata();
        float image_2_nodata = image_2_texture.nodata();

        glBindImageTexture(0, image_1_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
        glBindImageTexture(1, image_2_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

        glUseProgram(computeProgram_);
        glUniform2i(glGetUniformLocation(computeProgram_, "uSize"), width, height);
        glUniform1i(glGetUniformLocation(computeProgram_, "uNumGroupsX"), groupsX);

        glDispatchCompute(groupsX, groupsY, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, error_ssbo_);
        float *partial_sum = (float *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                                       numGroups * sizeof(float),
                                                       GL_MAP_READ_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_ssbo_);
        int *partial_count = (int *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                                     numGroups * sizeof(int),
                                                     GL_MAP_READ_BIT);

        double sum = 0.0;
        int total_count = 0;
        for (int i = 0; i < numGroups; i++)
        {
            sum += (double)partial_sum[i];
            total_count += partial_count[i];
        }

        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        error = std::sqrt(sum / total_count);
    }

private:
    GLuint program_;
};

class TextureBWRendererGL
{
public:
    TextureBWRendererGL()
    {
        const char *compute_shader = R"Shader(
            #version 460 core
            #extension GL_ARB_shader_atomic_float : require

            layout(local_size_x=16, local_size_y=16, local_size_z=1) in;

            layout(std430, binding=0) readonly buffer TexBuf { vec4 tex[]; };
            layout(std430, binding=1) buffer GradTexBuf { float gradTex[]; }; // packed as vec4 components: 4 floats per texel

            layout(std430, binding=2) readonly buffer UvBuf { vec2 uv[]; };
            layout(std430, binding=3) readonly buffer UvDaBuf { vec4 uv_da[]; };

            layout(std430, binding=4) readonly buffer DOutBuf { vec4 dOut[]; }; // dL/d(outColor)
            layout(std430, binding=5) buffer GradUvBuf { vec2 gradUV[]; };      // accumulate dL/d(uv)

            uniform ivec2 u_resolution;
            uniform int   u_depth;

            uniform int   u_mipLevelMax;
            uniform int   u_boundaryMode;
            uniform bool  u_enableMip;
            uniform bool  u_enableTrilinear;

            uniform ivec2 u_mipSize[16];
            uniform int   u_mipOffset[16];

            bool applyBoundary(inout ivec2 xy, ivec2 size, int boundaryMode) {
                if (boundaryMode == 0) { // wrap
                    xy = ivec2((xy.x % size.x + size.x) % size.x,
                            (xy.y % size.y + size.y) % size.y);
                    return true;
                }
                if (boundaryMode == 1) { // clamp
                    xy = clamp(xy, ivec2(0), size - ivec2(1));
                    return true;
                }
                return (xy.x >= 0 && xy.y >= 0 && xy.x < size.x && xy.y < size.y); // zero
            }

            int idxTexel(int offsetTexels, ivec2 size, int layer, ivec2 xy) {
                return offsetTexels + (layer * size.x * size.y) + (xy.x + size.x * xy.y);
            }

            void atomicAddVec4Texel(int texelIndex, vec4 g) {
                // gradTex packed as 4 floats per texel
                int base = texelIndex * 4;
                atomicAdd(gradTex[base + 0], g.x);
                atomicAdd(gradTex[base + 1], g.y);
                atomicAdd(gradTex[base + 2], g.z);
                atomicAdd(gradTex[base + 3], g.w);
            }

            float computeLod(vec2 dUVdx, vec2 dUVdy, ivec2 size) {
                vec2 dx = dUVdx * vec2(size);
                vec2 dy = dUVdy * vec2(size);
                float rho = max(length(dx), length(dy));
                return log2(max(rho, 1e-8));
            }

            struct TapInfo {
                ivec2 p00, p10, p01, p11;
                ivec2 q00, q10, q01, q11;
                bool ok00, ok10, ok01, ok11;
                vec2 f;
            };

            vec4 sampleAndTaps(int level, int layer, vec2 UV, out TapInfo info) {
                ivec2 size = u_mipSize[level];
                int base   = u_mipOffset[level];

                vec2 p = UV * vec2(size) - vec2(0.5);
                ivec2 i0 = ivec2(floor(p));
                vec2  f  = fract(p);
                info.f = f;

                info.p00 = i0 + ivec2(0,0);
                info.p10 = i0 + ivec2(1,0);
                info.p01 = i0 + ivec2(0,1);
                info.p11 = i0 + ivec2(1,1);

                info.q00 = info.p00; info.q10 = info.p10; info.q01 = info.p01; info.q11 = info.p11;
                info.ok00 = applyBoundary(info.q00, size, u_boundaryMode);
                info.ok10 = applyBoundary(info.q10, size, u_boundaryMode);
                info.ok01 = applyBoundary(info.q01, size, u_boundaryMode);
                info.ok11 = applyBoundary(info.q11, size, u_boundaryMode);

                vec4 c00 = info.ok00 ? tex[idxTexel(base, size, layer, info.q00)] : vec4(0.0);
                vec4 c10 = info.ok10 ? tex[idxTexel(base, size, layer, info.q10)] : vec4(0.0);
                vec4 c01 = info.ok01 ? tex[idxTexel(base, size, layer, info.q01)] : vec4(0.0);
                vec4 c11 = info.ok11 ? tex[idxTexel(base, size, layer, info.q11)] : vec4(0.0);

                vec4 cx0 = mix(c00, c10, f.x);
                vec4 cx1 = mix(c01, c11, f.x);
                return mix(cx0, cx1, f.y);
            }

            // Backprop through bilinear sample (no boundary gradients; taps outside -> contribute 0)
            void bilinearBackward(int level, int layer, vec2 UV, vec4 gOut, TapInfo info) {
                ivec2 size = u_mipSize[level];
                int base   = u_mipOffset[level];
                vec2 f = info.f;

                // Weights
                float w00 = (1.0 - f.x) * (1.0 - f.y);
                float w10 = (      f.x) * (1.0 - f.y);
                float w01 = (1.0 - f.x) * (      f.y);
                float w11 = (      f.x) * (      f.y);

                // Accumulate grad into texels
                if (info.ok00) atomicAddVec4Texel(idxTexel(base, size, layer, info.q00), w00 * gOut);
                if (info.ok10) atomicAddVec4Texel(idxTexel(base, size, layer, info.q10), w10 * gOut);
                if (info.ok01) atomicAddVec4Texel(idxTexel(base, size, layer, info.q01), w01 * gOut);
                if (info.ok11) atomicAddVec4Texel(idxTexel(base, size, layer, info.q11), w11 * gOut);

                // Grad wrt f.x and f.y uses neighboring differences
                vec4 c00 = info.ok00 ? tex[idxTexel(base, size, layer, info.q00)] : vec4(0.0);
                vec4 c10 = info.ok10 ? tex[idxTexel(base, size, layer, info.q10)] : vec4(0.0);
                vec4 c01 = info.ok01 ? tex[idxTexel(base, size, layer, info.q01)] : vec4(0.0);
                vec4 c11 = info.ok11 ? tex[idxTexel(base, size, layer, info.q11)] : vec4(0.0);

                // dC/dfx = (1-fy)*(c10-c00) + fy*(c11-c01)
                vec4 dCdfx = (1.0 - f.y) * (c10 - c00) + f.y * (c11 - c01);
                // dC/dfy = (1-fx)*(c01-c00) + fx*(c11-c10)
                vec4 dCdfy = (1.0 - f.x) * (c01 - c00) + f.x * (c11 - c10);

                float gfx = dot(gOut, dCdfx); // assumes output is RGBA; dot is fine as long as loss uses all channels similarly
                float gfy = dot(gOut, dCdfy);

                // f = fract(UV*size - 0.5) => df/dUV = size
                vec2 gUV = vec2(gfx, gfy) * vec2(size);

                // Accumulate UV gradient (atomic not needed if one thread per pixel, but safe if reused)
                atomicAdd(gradUV[0 + (layer * (u_resolution.x*u_resolution.y) + (int(gl_GlobalInvocationID.x) + u_resolution.x * int(gl_GlobalInvocationID.y)))].x, gUV.x);
                atomicAdd(gradUV[0 + (layer * (u_resolution.x*u_resolution.y) + (int(gl_GlobalInvocationID.x) + u_resolution.x * int(gl_GlobalInvocationID.y)))].y, gUV.y);
            }

            void main() {
                int px = int(gl_GlobalInvocationID.x);
                int py = int(gl_GlobalInvocationID.y);
                int pz = int(gl_GlobalInvocationID.z);

                int W = u_resolution.x, H = u_resolution.y;
                if (px >= W || py >= H || pz >= u_depth) return;

                int pidx = px + W * (py + H * pz);

                vec2 UV = uv[pidx];
                vec4 g  = dOut[pidx];

                int level0 = 0, level1 = 0;
                float t = 0.0;

                if (u_enableMip) {
                    vec4 da = uv_da[pidx];
                    vec2 dUVdx = vec2(da.x, da.z);
                    vec2 dUVdy = vec2(da.y, da.w);

                    float lod = computeLod(dUVdx, dUVdy, u_mipSize[0]);
                    lod = clamp(lod, 0.0, float(u_mipLevelMax));

                    if (u_enableTrilinear) {
                        float fl = floor(lod);
                        level0 = int(fl);
                        level1 = min(level0 + 1, u_mipLevelMax);
                        t = lod - fl; // stop-grad through t
                    } else {
                        level0 = int(round(lod));
                        level1 = level0;
                        t = 0.0;
                    }
                }

                // Level0
                TapInfo info0;
                sampleAndTaps(level0, pz, UV, info0);
                vec4 g0 = (level1 == level0) ? g : (1.0 - t) * g;
                bilinearBackward(level0, pz, UV, g0, info0);

                // Level1
                if (level1 != level0) {
                    TapInfo info1;
                    sampleAndTaps(level1, pz, UV, info1);
                    vec4 g1 = t * g;
                    bilinearBackward(level1, pz, UV, g1, info1);
                }
            }
            )Shader";

        program_ = create_program(compute_shader);
    }

    void Render(const MeshGL &mesh,
                int out_lvl,
                const TextureGL<Vec4<float>> &rast_texture,
                const TextureGL<Vec2<float>> &dy_texture,
                const BufferGL<float> pos_buffer,
                const BufferGL<int> tri_buffer,
                BufferGL<float> grad_buffer)
    {
        int width = image_1_texture.width(lvl);
        int height = image_1_texture.height(lvl);

        int groupsX = (width + 15) / 16;
        int groupsY = (height + 15) / 16;
        int numGroups = groupsX * groupsY;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, error_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numGroups * sizeof(float), nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, error_ssbo_);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numGroups * sizeof(int), nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, count_ssbo_);

        float image_1_nodata = image_1_texture.nodata();
        float image_2_nodata = image_2_texture.nodata();

        glBindImageTexture(0, image_1_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
        glBindImageTexture(1, image_2_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

        glUseProgram(computeProgram_);
        glUniform2i(glGetUniformLocation(computeProgram_, "uSize"), width, height);
        glUniform1i(glGetUniformLocation(computeProgram_, "uNumGroupsX"), groupsX);

        glDispatchCompute(groupsX, groupsY, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, error_ssbo_);
        float *partial_sum = (float *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                                       numGroups * sizeof(float),
                                                       GL_MAP_READ_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_ssbo_);
        int *partial_count = (int *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                                     numGroups * sizeof(int),
                                                     GL_MAP_READ_BIT);

        double sum = 0.0;
        int total_count = 0;
        for (int i = 0; i < numGroups; i++)
        {
            sum += (double)partial_sum[i];
            total_count += partial_count[i];
        }

        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        error = std::sqrt(sum / total_count);
    }

private:
    GLuint program_;
};
*/
