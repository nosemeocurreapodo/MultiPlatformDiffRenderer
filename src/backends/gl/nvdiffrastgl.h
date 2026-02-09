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

class RasterFWRendererGL
{
public:
    RasterFWRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 460 core

            layout(location=0) in vec4 in_pos; // object/world (NOT clip)

            uniform mat4 u_pose; // model->camera or model->world (depends on you)
            uniform mat4 u_proj; // camera projection (or view-proj)

            void main() {
                gl_Position = u_proj * u_pose * in_pos;
            }
            )Shader";

        const char *geometry_shader = R"Shader(
            #version 460 core
            layout(triangles) in;
            layout(triangle_strip, max_vertices=3) out;

            layout(location=0) flat out vec4 g_p0;
            layout(location=1) flat out vec4 g_p1;
            layout(location=2) flat out vec4 g_p2;
            layout(location=3) flat out int  g_triId1;

            uniform int u_triBase;

            void main() {
                vec4 p0 = gl_in[0].gl_Position;
                vec4 p1 = gl_in[1].gl_Position;
                vec4 p2 = gl_in[2].gl_Position;

                int triId1 = gl_PrimitiveIDIn + u_triBase + 1;

                for (int i = 0; i < 3; ++i) {
                    g_p0 = p0; g_p1 = p1; g_p2 = p2;
                    g_triId1 = triId1;

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

            layout(location=0) out vec4 out_rast; // RGBA32F
            layout(location=1) out vec4 out_db;   // RGBA32F

            uniform ivec2 u_resolution;

            void main() {
                int W = u_resolution.x;
                int H = u_resolution.y;

                // gl_FragCoord is already at pixel center (i+0.5,j+0.5)
                float fx = (2.0 * gl_FragCoord.x / float(W)) - 1.0;
                float fy = (2.0 * gl_FragCoord.y / float(H)) - 1.0;

                vec4 p0 = g_p0, p1 = g_p1, p2 = g_p2;

                float p0x = p0.x - fx * p0.w;
                float p0y = p0.y - fy * p0.w;
                float p1x = p1.x - fx * p1.w;
                float p1y = p1.y - fy * p1.w;
                float p2x = p2.x - fx * p2.w;
                float p2y = p2.y - fy * p2.w;

                float a0 = p1x*p2y - p1y*p2x;
                float a1 = p2x*p0y - p2y*p0x;
                float a2 = p0x*p1y - p0y*p1x;

                float iw = 1.0 / (a0 + a1 + a2);
                float b0 = a0 * iw;
                float b1 = a1 * iw;

                float z  = p0.z * a0 + p1.z * a1 + p2.z * a2;
                float w  = p0.w * a0 + p1.w * a1 + p2.w * a2;
                float zw = z / w;

                b0 = clamp(b0, 0.0, 1.0);
                b1 = clamp(b1, 0.0, 1.0);
                float bs = 1.0 / max(b0 + b1, 1.0);
                b0 *= bs; b1 *= bs;
                zw = clamp(zw, -1.0, 1.0);

                // store tri id numerically (exact up to ~16M)
                out_rast = vec4(b0, b1, zw, float(g_triId1));

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
                TextureGL<Vec4<float>> &rast_texture,
                TextureGL<Vec4<float>> &rast_db_texture)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

        // layered attachments (2D array textures)
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rast_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, rast_db_texture.id(), out_lvl);

        // make sure your create_framebuffer() attached a depth renderbuffer/texture.
        // If not, you MUST attach one here (GL_DEPTH_COMPONENT24 is fine).

        const GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, bufs);

        check_framebuffer();

        glDisable(GL_CULL_FACE); // nvdiffrast default: no culling
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        const GLsizei W = (GLsizei)rast_texture.width(out_lvl);
        const GLsizei H = (GLsizei)rast_texture.height(out_lvl);
        glViewport(0, 0, W, H);

        // Clear both color targets and depth.
        Vec4<float> rast_nodata = rast_texture.nodata();
        float clear0[4] = {rast_nodata(0), rast_nodata(1), rast_nodata(2), rast_nodata(3)};
        float clear1[4] = {0, 0, 0, 0}; // or rast_db_texture.nodata()

        glClearBufferfv(GL_COLOR, 0, clear0);
        glClearBufferfv(GL_COLOR, 1, clear1);
        glClear(GL_DEPTH_BUFFER_BIT);

        glUseProgram(program_);

        // SET THE UNIFORMS YOUR SHADERS ACTUALLY HAVE:
        Mat4<float> proj = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE,
                                                   RenderConstants::FAR_PLANE);
        Mat4<float> poseM = pose.matrix();

        glUniformMatrix4fv(glGetUniformLocation(program_, "u_proj"), 1, GL_FALSE, proj.data());
        glUniformMatrix4fv(glGetUniformLocation(program_, "u_pose"), 1, GL_FALSE, poseM.data());
        glUniform2i(glGetUniformLocation(program_, "u_resolution"), W, H);
        glUniform1i(glGetUniformLocation(program_, "u_triBase"), 0);

        mesh.draw();

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
        //#extension GL_ARB_shader_atomic_float : require

        layout(local_size_x=16, local_size_y=16, local_size_z=1) in;

        layout(rgba32f, binding=0) uniform readonly image2DArray u_rast; // (b0,b1,zw,triBits)
        layout(rg32f,   binding=1) uniform readonly image2DArray u_dy;   // (dy0,dy1)

        layout(std430, binding=0) readonly buffer PosBuf { vec4 pos[]; };
        layout(std430, binding=1) readonly buffer TriBuf { int  tri[]; };

        //layout(std430, binding=2) buffer GradBuf { float grad[]; };
        layout(std430, binding=2) buffer GradBuf { coherent volatile uint gradBits[]; };
        
        //uniform int   u_numTriangles;
        uniform ivec2 u_resolution;      // (W,H)

        int float_to_triidx(float fBitsAsFloat) {
            return int(floatBitsToUint(fBitsAsFloat)); // 1-based; 0 means empty
        }

        void atomicAddFloatBits(int idx, float v)
        {
            uint old = gradBits[idx];
            while (true)
            {
                uint assumed = old;
                float f = uintBitsToFloat(assumed) + v;
                uint desired = floatBitsToUint(f);
                old = atomicCompSwap(gradBits[idx], assumed, desired);
                if (old == assumed) break; // success
            }
        }

        void atomicAdd_xyw(int vi, float gx, float gy, float gw)
        {
            int base = 4 * vi;
            atomicAddFloatBits(base + 0, gx);
            atomicAddFloatBits(base + 1, gy);
            atomicAddFloatBits(base + 3, gw);
        }

        //void atomicAdd_xyw(int vi, float gx, float gy, float gw) {
        //    int base = 4 * vi;
        //    atomicAdd(grad[base + 0], gx);
        //    atomicAdd(grad[base + 1], gy);
        //    atomicAdd(grad[base + 3], gw);
        //}

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
            //if (triIdx < 0 || triIdx >= u_numTriangles) return;

            int vi0 = tri[triIdx * 3 + 0];
            int vi1 = tri[triIdx * 3 + 1];
            int vi2 = tri[triIdx * 3 + 2];

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
                const BufferGL<float, GL_ARRAY_BUFFER> &pos_buffer,
                const BufferGL<int, GL_ELEMENT_ARRAY_BUFFER> &tri_buffer,
                BufferGL<float> &grad_buffer) // number of layers in the 2DArray (pz)
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

        // glUniform1i(glGetUniformLocation(program_, "u_numTriangles"), numTriangles);
        // glUniform1i(glGetUniformLocation(program_, "u_numVertices"), numVerticesPerInstance);
        // glUniform1i(glGetUniformLocation(program_, "u_instanceMode"), instanceMode ? 1 : 0);
        // glUniform2i(glGetUniformLocation(program_, "u_resolution"), width, height);

        // Dispatch across layers so pz is valid.
        glDispatchCompute(groupsX, groupsY, 1);

        // Make SSBO writes visible to subsequent passes / readbacks.
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

private:
    GLuint program_ = 0;
};

class InterpolationFWRendererGL
{
public:
    InterpolationFWRendererGL()
    {
        const char *compute_shader = R"Shader(
            #version 460 core
            #ifndef ENABLE_DA
            #define ENABLE_DA 0
            #endif

            layout(local_size_x=16, local_size_y=16, local_size_z=1) in;

            layout(rgba32f, binding=0) uniform readonly image2DArray u_rast;
            #if ENABLE_DA
            layout(rgba32f, binding=1) uniform readonly image2DArray u_rastDB;
            #endif

            layout(std430, binding=0) readonly buffer TriBuf  { int   tri[];  };
            layout(std430, binding=1) readonly buffer AttrBuf { float attr[]; };

            layout(std430, binding=2) buffer OutBuf { float outAttr[]; };

            #if ENABLE_DA
            layout(std430, binding=3) buffer OutDABuf { vec2 outDA[]; };
            layout(std430, binding=4) readonly buffer DiffIdxBuf { int diffIdx[]; };
            #endif

            uniform ivec2 u_resolution;     // (W,H)
            uniform int   u_numTriangles;
            uniform int   u_numAttr;
            uniform int   u_numDiffAttr;

            uniform bool  u_attrBC;
            #if ENABLE_DA
            uniform bool  u_diff_attrs_all;
            #endif

            int float_to_triidx(float fBits) { return int(floatBitsToUint(fBits)); } // 1-based; 0 => none

            void writeZero(int pidx)
            {
                int outBase = pidx * u_numAttr;
                for (int i=0; i<u_numAttr; ++i) outAttr[outBase + i] = 0.0;

            #if ENABLE_DA
                int daBase = pidx * u_numDiffAttr;
                for (int i=0; i<u_numDiffAttr; ++i) outDA[daBase + i] = vec2(0.0);
            #endif
            }

            void main()
            {
                int px = int(gl_GlobalInvocationID.x);
                int py = int(gl_GlobalInvocationID.y);
                int pz = int(gl_GlobalInvocationID.z);

                int W = u_resolution.x;
                int H = u_resolution.y;
                if (px >= W || py >= H) return;

                int pidx = px + W * (py + H * pz);

                vec4 r = imageLoad(u_rast, ivec3(px, py, pz));
                int triIdx = float_to_triidx(r.w) - 1;
                bool triValid = (triIdx >= 0 && triIdx < u_numTriangles);

                if (!triValid) { writeZero(pidx); return; }

                int vi0 = tri[triIdx * 3 + 0];
                int vi1 = tri[triIdx * 3 + 1];
                int vi2 = tri[triIdx * 3 + 2];

                // If corrupt indices, zero output (don’t leave stale data).
                //if (vi0 < 0 || vi0 >= u_numVertices ||
                //    vi1 < 0 || vi1 >= u_numVertices ||
                //    vi2 < 0 || vi2 >= u_numVertices) {
                //    writeZero(pidx);
                //    return;
                //}

                int a0Base = vi0 * u_numAttr;
                int a1Base = vi1 * u_numAttr;
                int a2Base = vi2 * u_numAttr;

                float b0 = r.x;
                float b1 = r.y;
                float b2 = 1.0 - r.x - r.y;

                int outBase = pidx * u_numAttr;
                for (int i=0; i<u_numAttr; ++i) {
                    float s0 = attr[a0Base + i];
                    float s1 = attr[a1Base + i];
                    float s2 = attr[a2Base + i];
                    outAttr[outBase + i] = b0*s0 + b1*s1 + b2*s2;
                }

            #if ENABLE_DA
                vec4 db = imageLoad(u_rastDB, ivec3(px, py, pz));
                float dudx = db.x, dudy = db.y, dvdx = db.z, dvdy = db.w;

                int daBase = pidx * u_numDiffAttr;
                for (int i=0; i<u_numDiffAttr; ++i) {
                    int j = u_diff_attrs_all ? i : diffIdx[i];
                    if (j < 0) j += u_numAttr;

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

    void Render(
        int mipLevel,
        const TextureGL<Vec4<float>> &rast,   // must be GL_TEXTURE_2D_ARRAY + RGBA32F
        const TextureGL<Vec4<float>> *rastDB, // optional, only if ENABLE_DA=1
        int numTriangles,
        int numAttr,
        int numDiffAttr,
        bool attrBC,
        bool diff_attrs_all,
        const BufferGL<int, GL_ELEMENT_ARRAY_BUFFER> &tri_ssbo, // binding=0
        const BufferGL<float> &attr_ssbo,                       // binding=1
        BufferGL<float> &outAttr_ssbo,                          // binding=2
        BufferGL<float> *outDA_ssbo,                            // binding=3 (stores vec2 packed as 2 floats)
        const BufferGL<int> *diffIdx_ssbo                       // binding=4
    )
    {
        int width = rast.width(mipLevel);
        int height = rast.height(mipLevel);

        glUseProgram(program_);

        // --- Bind images (IMPORTANT: layered + correct format) ---
        glBindImageTexture(0, rast.id(), mipLevel, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);

        // If ENABLE_DA in shader, you must bind rastDB at binding=1 too.
        // (If you compiled ENABLE_DA=0, don’t bind it.)
        if (rastDB)
            glBindImageTexture(1, rastDB->id(), mipLevel, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);

        // --- Bind SSBOs (match bindings in GLSL) ---
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, tri_ssbo.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, attr_ssbo.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, outAttr_ssbo.id());
        if (outDA_ssbo)
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outDA_ssbo->id());
        if (diffIdx_ssbo)
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, diffIdx_ssbo->id());

        // --- Allocate output buffers if needed ---
        // outAttr: (W*H*D)*numAttr floats
        size_t outAttrBytes = size_t(width) * height * numAttr * sizeof(float);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, outAttr_ssbo.id());
        glBufferData(GL_SHADER_STORAGE_BUFFER, outAttrBytes, nullptr, GL_DYNAMIC_DRAW);

        // outDA: (W*H*D)*numDiffAttr vec2 => 2 floats
        if (outDA_ssbo)
        {
            size_t outDABytes = size_t(width) * height * numDiffAttr * 2 * sizeof(float);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, outDA_ssbo->id());
            glBufferData(GL_SHADER_STORAGE_BUFFER, outDABytes, nullptr, GL_DYNAMIC_DRAW);
        }

        // --- Uniforms (names MUST match shader) ---
        glUniform2i(glGetUniformLocation(program_, "u_resolution"), width, height);
        glUniform1i(glGetUniformLocation(program_, "u_numTriangles"), numTriangles);
        glUniform1i(glGetUniformLocation(program_, "u_numAttr"), numAttr);
        glUniform1i(glGetUniformLocation(program_, "u_numDiffAttr"), numDiffAttr);

        glUniform1i(glGetUniformLocation(program_, "u_attrBC"), attrBC ? 1 : 0);

        // Only if ENABLE_DA=1 in this compiled program
        GLint locAll = glGetUniformLocation(program_, "u_diff_attrs_all");
        if (locAll >= 0)
            glUniform1i(locAll, diff_attrs_all ? 1 : 0);

        // --- Dispatch (IMPORTANT: z = depth) ---
        int groupsX = (width + 15) / 16;
        int groupsY = (height + 15) / 16;
        glDispatchCompute(groupsX, groupsY, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

private:
    GLuint program_ = 0;
};
/*
class InterpolationBWRendererGL
{
public:
    InterpolationBWRendererGL()
    {
        const char *compute_shader = R"Shader(
            #version 460 core

            #ifndef ENABLE_DA
            #define ENABLE_DA 0
            #endif

            layout(local_size_x=16, local_size_y=16, local_size_z=1) in;

            // Inputs
            layout(rgba32f, binding=0) uniform readonly image2DArray u_rast;     // (b0,b1,zw,triIdBits)

            #if ENABLE_DA
            layout(rgba32f, binding=1) uniform readonly image2DArray u_rastDB;   // (dudx,dudy,dvdx,dvdy)
            #endif

            // Outputs (match CUDA)
            layout(rgba32f, binding=2) uniform writeonly image2DArray u_gradRaster;    // (gb0,gb1,0,0)
            #if ENABLE_DA
            layout(rgba32f, binding=3) uniform writeonly image2DArray u_gradRasterDB;  // (gdudx,gdudy,gdvdx,gdvdy)
            #endif

            // Geometry + attributes
            layout(std430, binding=0) readonly buffer TriBuf  { int   tri[]; };
            layout(std430, binding=1) readonly buffer AttrBuf { float attr[]; };

            // Incoming grads
            layout(std430, binding=2) readonly buffer DyBuf   { float dy[]; }; // (W*H*D)*numAttr

            #if ENABLE_DA
            layout(std430, binding=3) readonly buffer DdaBuf      { vec2  dda[]; };    // (W*H*D)*numDiffAttr
            layout(std430, binding=4) readonly buffer DiffIdxBuf  { int   diffIdx[]; };
            #endif

            // Accumulated output grads (same byte size as float[], but stored as uint bits)
            layout(std430, binding=5) buffer GradAttrBuf { coherent volatile uint gradAttrBits[]; };

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

            int float_to_triidx(float fBits) { return int(floatBitsToUint(fBits)); } // 1-based, 0 means none

            // Float atomicAdd via CAS on uint bits.
            void atomicAddFloatBits(int idx, float v)
            {
                uint old = gradAttrBits[idx];
                while (true)
                {
                    uint assumed = old;
                    float f = uintBitsToFloat(assumed) + v;
                    uint desired = floatBitsToUint(f);
                    old = atomicCompSwap(gradAttrBits[idx], assumed, desired);
                    if (old == assumed) break;
                }
            }

            void atomicAddAttr(int vi, int ai, float v)
            {
                atomicAddFloatBits(vi * u_numAttr + ai, v);
            }

            void main()
            {
                int px = int(gl_GlobalInvocationID.x);
                int py = int(gl_GlobalInvocationID.y);
                int pz = int(gl_GlobalInvocationID.z);

                int W = u_resolution.x;
                int H = u_resolution.y;
                if (px >= W || py >= H || pz >= u_depth) return;

                int pidx = px + W * (py + H * pz);

                vec4 r = imageLoad(u_rast, ivec3(px, py, pz));
                int triIdx = float_to_triidx(r.w) - 1;

                // If no triangle: write zero grads and exit (CUDA behavior).
                if (triIdx < 0 || triIdx >= u_numTriangles)
                {
                    imageStore(u_gradRaster, ivec3(px, py, pz), vec4(0.0));
                #if ENABLE_DA
                    imageStore(u_gradRasterDB, ivec3(px, py, pz), vec4(0.0));
                #endif
                    return;
                }

                // Vertex indices
                int vi0 = tri[triIdx * 3 + 0];
                int vi1 = tri[triIdx * 3 + 1];
                int vi2 = tri[triIdx * 3 + 2];

                // Corrupt indices => bail (CUDA bails without writing gradRaster; but this is rare)
                if (vi0 < 0 || vi0 >= u_numVertices ||
                    vi1 < 0 || vi1 >= u_numVertices ||
                    vi2 < 0 || vi2 >= u_numVertices)
                    return;

                // Instance mode: adjust unless broadcasting attributes
                if (u_instanceMode && !u_attrBC)
                {
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

                float gb0 = 0.0;
                float gb1 = 0.0;

                // Main attribute loop (matches CUDA)
                for (int i = 0; i < u_numAttr; ++i)
                {
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

                for (int i = 0; i < u_numDiffAttr; ++i)
                {
                    int j = u_diff_attrs_all ? i : diffIdx[i];
                    if (j < 0) j += u_numAttr;

                    if (j >= 0 && j < u_numAttr)
                    {
                        vec2 g = dda[ddaBase + i];
                        float dsdx = g.x;
                        float dsdy = g.y;

                        float s0 = attr[a0Base + j];
                        float s1 = attr[a1Base + j];
                        float s2 = attr[a2Base + j];

                        float dsdu = s0 - s2;
                        float dsdv = s1 - s2;

                        gdudx += dsdu * dsdx;
                        gdudy += dsdu * dsdy;
                        gdvdx += dsdv * dsdx;
                        gdvdy += dsdv * dsdy;

                        float du = dsdx * dudx + dsdy * dudy;
                        float dv = dsdx * dvdx + dsdy * dvdy;

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

    void Render(
        int lvl,
        int W, int H, int D,
        const TextureGL<Vec4<float>> &rastTex,   // 2D array, RGBA32F
        const TextureGL<Vec4<float>> &rastDBTex, // 2D array, RGBA32F (if DA)
        TextureGL<Vec4<float>> &gradRasterTex,   // 2D array, RGBA32F
        TextureGL<Vec4<float>> &gradRasterDBTex, // 2D array, RGBA32F (if DA)
        const BufferGL<int> &triSSBO,
        const BufferGL<float> &attrSSBO,
        const BufferGL<float> &dySSBO,
        const BufferGL<uint32_t> &gradAttrBitsSSBO, // IMPORTANT: uint storage
        // optional DA:
        const BufferGL<Vec2<float>> &ddaSSBO,
        const BufferGL<int> &diffIdxSSBO,
        bool enableDA,
        int numTriangles,
        int numVerticesPerInst,
        int numAttr,
        int numDiffAttr,
        bool instanceMode,
        bool attrBC,
        bool diffAttrsAll)
    {
        glUseProgram(program_);

        // Images: NOTE layered = GL_TRUE for 2DArray
        glBindImageTexture(0, rastTex.id(), lvl, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
        if (enableDA)
            glBindImageTexture(1, rastDBTex.id(), lvl, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);

        glBindImageTexture(2, gradRasterTex.id(), lvl, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        if (enableDA)
            glBindImageTexture(3, gradRasterDBTex.id(), lvl, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        // SSBOs
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, triSSBO.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, attrSSBO.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, dySSBO.id());
        if (enableDA)
        {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ddaSSBO.id());
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, diffIdxSSBO.id());
        }
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, gradAttrBitsSSBO.id());

        // Clear gradAttrBits to 0 (uint zeros).
        {
            GLuint zero = 0u;
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, gradAttrBitsSSBO.id());
            glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
        }

        // Uniforms
        glUniform2i(glGetUniformLocation(program_, "u_resolution"), W, H);
        glUniform1i(glGetUniformLocation(program_, "u_depth"), D);
        glUniform1i(glGetUniformLocation(program_, "u_numTriangles"), numTriangles);
        glUniform1i(glGetUniformLocation(program_, "u_numVertices"), numVerticesPerInst);
        glUniform1i(glGetUniformLocation(program_, "u_numAttr"), numAttr);
        glUniform1i(glGetUniformLocation(program_, "u_numDiffAttr"), numDiffAttr);
        glUniform1i(glGetUniformLocation(program_, "u_instanceMode"), instanceMode ? 1 : 0);
        glUniform1i(glGetUniformLocation(program_, "u_attrBC"), attrBC ? 1 : 0);
        if (enableDA)
            glUniform1i(glGetUniformLocation(program_, "u_diff_attrs_all"), diffAttrsAll ? 1 : 0);

        int groupsX = (W + 15) / 16;
        int groupsY = (H + 15) / 16;

        // IMPORTANT: z = D
        glDispatchCompute(groupsX, groupsY, D);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

private:
    GLuint program_;
};
*/
/*
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
            layout(std430, binding=1) readonly buffer UvBuf   { vec2 uv[]; };
            layout(std430, binding=2) readonly buffer UvDaBuf { vec4 uv_da[]; }; // (dudx,dudy,dvdx,dvdy)

            layout(std430, binding=3) buffer OutBuf { vec4 outColor[]; };

            uniform ivec2 u_resolution;
            uniform int   u_depth;

            uniform int   u_mipLevelMax;
            uniform int   u_boundaryMode;     // 0=wrap,1=clamp,2=zero
            uniform bool  u_enableMip;
            uniform bool  u_enableTrilinear;

            uniform ivec2 u_mipSize[16];
            uniform int   u_mipOffset[16];

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
                // zero
                return (xy.x >= 0 && xy.y >= 0 && xy.x < size.x && xy.y < size.y);
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

    void Render(
        int W, int H, int D,
        const BufferGL<float> &tex_ssbo,  // binding=0, stores vec4 => float[4*N]
        const BufferGL<float> &uv_ssbo,   // binding=1, stores vec2 => float[2*(W*H*D)]
        const BufferGL<float> &uvda_ssbo, // binding=2, stores vec4 => float[4*(W*H*D)] (bind a dummy if !enableMip)
        BufferGL<float> &out_ssbo,        // binding=3, stores vec4 => float[4*(W*H*D)]
        int mipLevelMax,
        int boundaryMode, // 0 wrap, 1 clamp, 2 zero
        bool enableMip,
        bool enableTrilinear,
        const std::array<glm::ivec2, 16> &mipSize,
        const std::array<int, 16> &mipOffset)
    {
        glUseProgram(program_);

        // SSBO bindings (match GLSL)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, tex_ssbo.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, uv_ssbo.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, uvda_ssbo.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, out_ssbo.id());

        // Uniforms
        glUniform2i(glGetUniformLocation(program_, "u_resolution"), W, H);
        glUniform1i(glGetUniformLocation(program_, "u_depth"), D);

        glUniform1i(glGetUniformLocation(program_, "u_mipLevelMax"), mipLevelMax);
        glUniform1i(glGetUniformLocation(program_, "u_boundaryMode"), boundaryMode);
        glUniform1i(glGetUniformLocation(program_, "u_enableMip"), enableMip ? 1 : 0);
        glUniform1i(glGetUniformLocation(program_, "u_enableTrilinear"), enableTrilinear ? 1 : 0);

        // Upload uniform arrays
        GLint locMipSize = glGetUniformLocation(program_, "u_mipSize[0]");
        GLint locMipOffset = glGetUniformLocation(program_, "u_mipOffset[0]");

        // ivec2 array -> upload as 2*16 ints
        int mipSizePacked[16 * 2];
        for (int i = 0; i < 16; ++i)
        {
            mipSizePacked[2 * i + 0] = mipSize[i].x;
            mipSizePacked[2 * i + 1] = mipSize[i].y;
        }
        glUniform2iv(locMipSize, 16, mipSizePacked);
        glUniform1iv(locMipOffset, 16, mipOffset.data());

        // Dispatch
        int groupsX = (W + 15) / 16;
        int groupsY = (H + 15) / 16;
        glDispatchCompute(groupsX, groupsY, D);

        // Out is SSBO, so SSBO barrier is enough
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

private:
    GLuint program_;
};
*/
/*
class TextureBWRendererGL
{
public:
    TextureBWRendererGL()
    {
        const char *compute_shader = R"Shader(
            #version 460 core

            layout(local_size_x=16, local_size_y=16, local_size_z=1) in;

            // Forward texture pyramid (packed)
            layout(std430, binding=0) readonly buffer TexBuf { vec4 tex[]; };

            // Grad wrt texels: 4 uints per texel (bitwise float). Same underlying buffer bytes as float[4*N] is fine.
            layout(std430, binding=1) buffer GradTexBuf { uint gradTexU[]; };

            layout(std430, binding=2) readonly buffer UvBuf   { vec2 uv[]; };
            layout(std430, binding=3) readonly buffer UvDaBuf { vec4 uv_da[]; }; // (dudx,dudy,dvdx,dvdy) optional if enableMip==false

            layout(std430, binding=4) readonly buffer DOutBuf { vec4 dOut[]; };   // dL/d(outColor)

            // Grad wrt uv per pixel (no atomics needed: one thread per pixel)
            layout(std430, binding=5) buffer GradUvBuf { vec2 gradUV[]; };

            uniform ivec2 u_resolution;
            uniform int   u_depth;

            uniform int   u_mipLevelMax;
            uniform int   u_boundaryMode;     // 0=wrap, 1=clamp, 2=zero
            uniform bool  u_enableMip;
            uniform bool  u_enableTrilinear;

            uniform ivec2 u_mipSize[16];
            uniform int   u_mipOffset[16];

            // -------------------- utils --------------------

            bool applyBoundary(inout ivec2 xy, ivec2 size, int boundaryMode)
            {
                if (boundaryMode == 0) { // wrap
                    xy = ivec2((xy.x % size.x + size.x) % size.x,
                            (xy.y % size.y + size.y) % size.y);
                    return true;
                }
                if (boundaryMode == 1) { // clamp
                    xy = clamp(xy, ivec2(0), size - ivec2(1));
                    return true;
                }
                // zero
                return (xy.x >= 0 && xy.y >= 0 && xy.x < size.x && xy.y < size.y);
            }

            int idxTexel(int offsetTexels, ivec2 size, int layer, ivec2 xy)
            {
                return offsetTexels + (layer * size.x * size.y) + (xy.x + size.x * xy.y);
            }

            // Atomic add for float stored as uint bits (CAS loop). Works without shader float atomic extensions.
            void atomicAddFloatCAS(inout uint addr, float v)
            {
                uint oldBits = addr;
                while (true)
                {
                    uint assumed = oldBits;
                    float oldF = uintBitsToFloat(assumed);
                    float newF = oldF + v;
                    uint  newBits = floatBitsToUint(newF);
                    oldBits = atomicCompSwap(addr, assumed, newBits);
                    if (oldBits == assumed) break;
                }
            }

            void atomicAddVec4Texel(int texelIndex, vec4 g)
            {
                int base = texelIndex * 4;
                atomicAddFloatCAS(gradTexU[base + 0], g.x);
                atomicAddFloatCAS(gradTexU[base + 1], g.y);
                atomicAddFloatCAS(gradTexU[base + 2], g.z);
                atomicAddFloatCAS(gradTexU[base + 3], g.w);
            }

            float computeLod(vec2 dUVdx, vec2 dUVdy, ivec2 size)
            {
                vec2 dx = dUVdx * vec2(size);
                vec2 dy = dUVdy * vec2(size);
                float rho = max(length(dx), length(dy));
                return log2(max(rho, 1e-8));
            }

            struct TapInfo {
                ivec2 q00, q10, q01, q11;
                bool ok00, ok10, ok01, ok11;
                vec2 f;
            };

            void makeTaps(int level, int layer, vec2 UV, out TapInfo info)
            {
                ivec2 size = u_mipSize[level];

                vec2 p = UV * vec2(size) - vec2(0.5);
                ivec2 i0 = ivec2(floor(p));
                vec2  f  = fract(p);
                info.f = f;

                ivec2 p00 = i0 + ivec2(0,0);
                ivec2 p10 = i0 + ivec2(1,0);
                ivec2 p01 = i0 + ivec2(0,1);
                ivec2 p11 = i0 + ivec2(1,1);

                info.q00 = p00; info.q10 = p10; info.q01 = p01; info.q11 = p11;
                info.ok00 = applyBoundary(info.q00, size, u_boundaryMode);
                info.ok10 = applyBoundary(info.q10, size, u_boundaryMode);
                info.ok01 = applyBoundary(info.q01, size, u_boundaryMode);
                info.ok11 = applyBoundary(info.q11, size, u_boundaryMode);
            }

            // Backprop bilinear. Also accumulates dL/dUV (ignoring gradients through wrap/clamp decisions).
            void bilinearBackward(int level, int layer, vec2 UV, vec4 gOut, TapInfo info, int pidx)
            {
                ivec2 size = u_mipSize[level];
                int base   = u_mipOffset[level];
                vec2 f     = info.f;

                float w00 = (1.0 - f.x) * (1.0 - f.y);
                float w10 = (      f.x) * (1.0 - f.y);
                float w01 = (1.0 - f.x) * (      f.y);
                float w11 = (      f.x) * (      f.y);

                // Texel grads
                if (info.ok00) atomicAddVec4Texel(idxTexel(base, size, layer, info.q00), w00 * gOut);
                if (info.ok10) atomicAddVec4Texel(idxTexel(base, size, layer, info.q10), w10 * gOut);
                if (info.ok01) atomicAddVec4Texel(idxTexel(base, size, layer, info.q01), w01 * gOut);
                if (info.ok11) atomicAddVec4Texel(idxTexel(base, size, layer, info.q11), w11 * gOut);

                // UV grads via df = size * dUV (ignoring boundary remap grads)
                vec4 c00 = info.ok00 ? tex[idxTexel(base, size, layer, info.q00)] : vec4(0.0);
                vec4 c10 = info.ok10 ? tex[idxTexel(base, size, layer, info.q10)] : vec4(0.0);
                vec4 c01 = info.ok01 ? tex[idxTexel(base, size, layer, info.q01)] : vec4(0.0);
                vec4 c11 = info.ok11 ? tex[idxTexel(base, size, layer, info.q11)] : vec4(0.0);

                vec4 dCdfx = (1.0 - f.y) * (c10 - c00) + f.y * (c11 - c01);
                vec4 dCdfy = (1.0 - f.x) * (c01 - c00) + f.x * (c11 - c10);

                float gfx = dot(gOut, dCdfx);
                float gfy = dot(gOut, dCdfy);

                vec2 gUV = vec2(gfx, gfy) * vec2(size);

                // One thread per pixel => no atomic needed
                gradUV[pidx] += gUV;
            }

            void main()
            {
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
                        t = lod - fl; // stop-grad through t/lod here (matches your intent)
                    } else {
                        level0 = int(round(lod));
                        level1 = level0;
                        t = 0.0;
                    }
                }

                // Level 0
                TapInfo info0;
                makeTaps(level0, pz, UV, info0);
                vec4 g0 = (level1 == level0) ? g : (1.0 - t) * g;
                bilinearBackward(level0, pz, UV, g0, info0, pidx);

                // Level 1
                if (level1 != level0) {
                    TapInfo info1;
                    makeTaps(level1, pz, UV, info1);
                    vec4 g1 = t * g;
                    bilinearBackward(level1, pz, UV, g1, info1, pidx);
                }
            }
            )Shader";

        program_ = create_program(compute_shader);
    }

    void Render(
        int W, int H, int D,
        const BufferGL<float> &tex_ssbo,  // binding=0 (vec4 tex[])
        BufferGL<float> &gradTex_ssbo,    // binding=1 (uint gradTexU[] but same bytes)
        const BufferGL<float> &uv_ssbo,   // binding=2 (vec2 uv[])
        const BufferGL<float> &uvda_ssbo, // binding=3 (vec4 uv_da[]) bind dummy if !enableMip
        const BufferGL<float> &dOut_ssbo, // binding=4 (vec4 dOut[])
        BufferGL<float> &gradUV_ssbo,     // binding=5 (vec2 gradUV[])
        int mipLevelMax,
        int boundaryMode,
        bool enableMip,
        bool enableTrilinear,
        const std::array<glm::ivec2, 16> &mipSize,
        const std::array<int, 16> &mipOffset)
    {
        glUseProgram(program_);

        // Bind SSBOs exactly as in GLSL.
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, tex_ssbo.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gradTex_ssbo.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, uv_ssbo.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, uvda_ssbo.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, dOut_ssbo.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, gradUV_ssbo.id());

        // Zero grads (important).
        // gradTex is 4 floats/texel => clear as R32UI over the whole buffer (works because we store float bits).
        {
            GLuint zero = 0;
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, gradTex_ssbo.id());
            glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

            float z2[2] = {0.f, 0.f};
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, gradUV_ssbo.id());
            glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_RG32F, GL_RG, GL_FLOAT, z2);
        }

        // Uniforms
        glUniform2i(glGetUniformLocation(program_, "u_resolution"), W, H);
        glUniform1i(glGetUniformLocation(program_, "u_depth"), D);

        glUniform1i(glGetUniformLocation(program_, "u_mipLevelMax"), mipLevelMax);
        glUniform1i(glGetUniformLocation(program_, "u_boundaryMode"), boundaryMode);
        glUniform1i(glGetUniformLocation(program_, "u_enableMip"), enableMip ? 1 : 0);
        glUniform1i(glGetUniformLocation(program_, "u_enableTrilinear"), enableTrilinear ? 1 : 0);

        // Arrays
        GLint locMipSize = glGetUniformLocation(program_, "u_mipSize[0]");
        GLint locMipOffset = glGetUniformLocation(program_, "u_mipOffset[0]");

        int mipSizePacked[16 * 2];
        for (int i = 0; i < 16; ++i)
        {
            mipSizePacked[2 * i + 0] = mipSize[i].x;
            mipSizePacked[2 * i + 1] = mipSize[i].y;
        }
        glUniform2iv(locMipSize, 16, mipSizePacked);
        glUniform1iv(locMipOffset, 16, mipOffset.data());

        // Dispatch
        int groupsX = (W + 15) / 16;
        int groupsY = (H + 15) / 16;
        glDispatchCompute(groupsX, groupsY, D);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

private:
    GLuint program_;
};
*/

static const char *nvdiffrast_common = R"GLSL(
        vec2 pointToPix(vec3 point, vec4 intrinsics)
        {
            vec2 pix;
            pix.x = (point.x / point.z) * intrinsics.x + intrinsics.z;
            pix.y = (point.y / point.z) * intrinsics.y + intrinsics.w;
            return pix;
        }
        )GLSL";

class RasterShadeFWRendererGL
{
public:
    RasterShadeFWRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 460 core

            layout(location=0) in vec3 in_pos; // object/world

            layout(location=0) out vec3 v_pos;

            uniform mat4 u_pose; // model->camera (or model->world if u_proj is view-proj)
            uniform mat4 u_proj; // projection (or view-proj)

            void main() {
                v_pos = in_pos;
                gl_Position = u_proj * u_pose * vec4(in_pos, 1.0);
            }
        )Shader";

        const char *geometry_shader = R"Shader(
            #version 460 core
            layout(triangles) in;
            layout(triangle_strip, max_vertices=3) out;

            layout(location=0) in vec3 v_pos[]; // match VS location

            layout(location=0) flat out uint g_triId1;
            layout(location=1) out vec3 g_pos;

            // IMPORTANT: nvdiffrast-style u,v should be screen-linear
            layout(location=2) noperspective out vec2 g_bary_uv; // (u,v) = weights for vertex0 and vertex1

            uniform int u_triBase;

            void main() {
                uint triId1 = uint(gl_PrimitiveIDIn + u_triBase + 1);
                g_triId1 = triId1;

                for (int i = 0; i < 3; ++i) {
                    g_pos = v_pos[i];

                    // nvdiffrast convention:
                    // u = weight(vertex0), v = weight(vertex1), w = 1-u-v = weight(vertex2)
                    // so per-vertex:
                    // v0: (1,0), v1: (0,1), v2: (0,0)
                    g_bary_uv = (i == 0) ? vec2(1.0, 0.0) :
                               (i == 1) ? vec2(0.0, 1.0) :
                                          vec2(0.0, 0.0);

                    gl_Position = gl_in[i].gl_Position;
                    EmitVertex();
                }
                EndPrimitive();
            }
        )Shader";

        const char *fragment_shader = R"Shader(
            #version 460 core

            layout(location=0) flat in uint g_triId1;
            layout(location=1) in vec3 g_pos;
            layout(location=2) noperspective in vec2 g_bary_uv;

            // If basecolor_texture is GL_R32F, writing a float is correct.
            layout(location=0) out float outBaseColor;
            layout(location=1) out vec4  out_rast; // RGBA32F: (u,v,depth,triIdBits)

            // Bind explicitly so you don't forget to set the uniform
            layout(binding=0) uniform sampler2D diffuseTex;

            uniform int u_inLvl; // optional mip level for the texture pyramid
            uniform vec4 u_intrinsics; // (fx,fy,cx,cy) for pointToPix

            void main() {

                vec2 texcoord = pointToPix(g_pos, u_intrinsics);
                
                if(texcoord.x < 0.0 || texcoord.x > 1.0 || texcoord.y < 0.0 || texcoord.y > 1.0)
                {
                    discard;
                }

                float u = g_bary_uv.x;
                float v = g_bary_uv.y;

                // gl_FragCoord.z is depth in [0,1] after projection/viewport (good enough for AA front/back).
                float depth01 = gl_FragCoord.z;

                // If you are building pyramids and want deterministic LOD:
                float albedo = textureLod(diffuseTex, texcoord, float(u_inLvl)).r;
                // If you don't want explicit LOD, replace with:
                // float albedo = texture(diffuseTex, texcoord).r;

                outBaseColor = albedo;

                // Pack tri id exactly into float bits (safe for any uint)
                float triBits = uintBitsToFloat(g_triId1);

                out_rast = vec4(u, v, depth01, triBits);
            }
        )Shader";

        // Create FBO + depth RBO (but we will RESIZE the RBO per Render() call!)
        create_framebuffer(fbo_, rbo_);

        program_ = create_program(vertex_shader, geometry_shader, fragment_shader, nvdiffrast_common);

        // No need to set sampler uniform because we used layout(binding=0).
        // If you remove layout(binding=0), then you must glUniform1i(diffuseTexLoc, 0).
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int lvl,
                const TextureGL<float> &diffuse_texture,
                TextureGL<float> &basecolor_texture,
                TextureGL<Vec4<float>> &rast_texture)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

        // Attach mip level = out_lvl.
        // If these are GL_TEXTURE_2D: glFramebufferTexture(..., level) is fine.
        // If these are GL_TEXTURE_2D_ARRAY and out_lvl is a LAYER, you must use glFramebufferTextureLayer instead.
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, basecolor_texture.id(), lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, rast_texture.id(), lvl);

        const GLsizei W = (GLsizei)rast_texture.width(lvl);
        const GLsizei H = (GLsizei)rast_texture.height(lvl);
        glViewport(0, 0, W, H);

        // VERY IMPORTANT: depth renderbuffer must match the render size (especially for mip rendering).
        //glBindRenderbuffer(GL_RENDERBUFFER, rbo_);
        //glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, W, H);
        //glBindRenderbuffer(GL_RENDERBUFFER, 0);

        // Make sure depth is attached (create_framebuffer should do this once; doing it again is harmless)
        //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_); // now actually attach it

        const GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, bufs);

        check_framebuffer();

        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        //glEnable(GL_CULL_FACE);
        //glEnable(GL_DEPTH_TEST);
        // glDisable(GL_SCISSOR_TEST);
        // this is because of the flip in y in the output image
        // so that we can display using opencv
        //glCullFace(GL_BACK);
        //glFrontFace(GL_CW); // was GL_CCW

        // Clear both color targets and depth.
        float basecolor_nodata = basecolor_texture.nodata();
        float clear0[4] = {basecolor_nodata, 0, 0, 0};

        // IMPORTANT: background triId should be 0 -> triBits = uintBitsToFloat(0)
        // so clear rast.w to uintBitsToFloat(0) to mark "no triangle"
        Vec4<float> rast_nodata = rast_texture.nodata();
        float clear1[4] = {rast_nodata(0), rast_nodata(1), rast_nodata(2), rast_nodata(3)}; // uintBitsToFloat(0u)};

        glClearBufferfv(GL_COLOR, 0, clear0);
        glClearBufferfv(GL_COLOR, 1, clear1);
        glClear(GL_DEPTH_BUFFER_BIT);

        glUseProgram(program_);

        // Bind diffuse texture to unit 0 (matches layout(binding=0))
        glBindTextureUnit(0, diffuse_texture.id());

        // Set uniforms
        Mat4<float> proj = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE,
                                                   RenderConstants::FAR_PLANE);
        Mat4<float> poseM = pose.matrix();

        // WARNING: if your Mat4 is row-major, you may need GL_TRUE instead of GL_FALSE.
        glUniformMatrix4fv(glGetUniformLocation(program_, "u_proj"), 1, GL_FALSE, proj.data());
        glUniformMatrix4fv(glGetUniformLocation(program_, "u_pose"), 1, GL_FALSE, poseM.data());
        glUniform1i(glGetUniformLocation(program_, "u_triBase"), 0);
        glUniform1i(glGetUniformLocation(program_, "u_inLvl"), lvl);
        glUniform4fv(glGetUniformLocation(program_, "u_intrinsics"), 1, cam.GetParams().data());

        mesh.draw();

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

private:
    GLuint fbo_ = 0;
    GLuint rbo_ = 0;
    GLuint program_ = 0;
};

class RasterShadeBWRendererGL
{
public:
    RasterShadeBWRendererGL()
    {
        const char *compute_shader = R"Shader(
            #version 460
            // #extension GL_EXT_shader_atomic_float : require
            // If EXT isn't available, try:
            // #extension GL_NV_shader_atomic_float : require

            layout(local_size_x=8, local_size_y=8) in;

            // Image units:
            //  0: rast (RGBA32F) = (u,v,depth,triIdBits OR float(triId1))
            //  1: dy   (RG32F)   = (dL/dbase, unused)
            layout(binding=0, rgba32f) readonly uniform image2D rastImg;
            layout(binding=1, rg32f)   readonly uniform image2D dyImg;

            // Sampler uses texture unit 0
            layout(binding=0) uniform sampler2D diffuseTex;
            uniform int  u_texLod;

            // Pose + intrinsics
            uniform mat4 u_pose;   // world -> cam
            uniform vec4 u_intr;   // (fx, fy, cx, cy)

            // SSBOs:
            // binding=0: vertex positions float array: [px,py,pz] * N
            layout(std430, binding=0) readonly buffer VertPosBuf { float vbuf[]; };
            // binding=1: triangle indices: [i0,i1,i2] * T
            layout(std430, binding=1) readonly buffer TriBuf     { int tbuf[]; };

            // binding=2: vertex position grads: [dpx,dpy,dpz] * N
            layout(std430, binding=2) buffer GradVertPosBuf      { coherent uint gV[]; };

            // binding=3: pose+intr grads (10 floats):
            // [dtx,dty,dtz, dωx,dωy,dωz, dfx,dfy,dcx,dcy]
            layout(std430, binding=3) buffer GradCamBuf          { coherent uint gC[]; };

            // ---------- helpers ----------
            float cross2(vec2 a, vec2 b) { return a.x*b.y - a.y*b.x; }
            vec2  perp(vec2 a) { return vec2(a.y, -a.x); }

            uint decodeTriId(float w)
            {
                // numeric float(triId) (exact up to 16,777,216)
                if (w > 0.5 && w < 16777216.0) {
                    float rw = round(w);
                    if (abs(w - rw) < 1e-3) return uint(rw);
                }
                return floatBitsToUint(w);
            }

            vec3 loadPos(int vid)
            {
                int base = vid * 3;
                return vec3(vbuf[base+0], vbuf[base+1], vbuf[base+2]);
            }

            float atomicAddFloatBitsAtV(int idx, float v)
            {
                uint oldBits = gVbits[idx];
                for (;;)
                {
                    float oldVal  = uintBitsToFloat(oldBits);
                    float newVal  = oldVal + v;
                    uint  newBits = floatBitsToUint(newVal);

                    uint prev = atomicCompSwap(gVbits[idx], oldBits, newBits);
                    if (prev == oldBits) return oldVal;
                    oldBits = prev;
                }
            }

            float atomicAddFloatBitsAtC(int idx, float v)
            {
                uint oldBits = gCbits[idx];
                for (;;)
                {
                    float oldVal  = uintBitsToFloat(oldBits);
                    float newVal  = oldVal + v;
                    uint  newBits = floatBitsToUint(newVal);

                    uint prev = atomicCompSwap(gCbits[idx], oldBits, newBits);
                    if (prev == oldBits) return oldVal;
                    oldBits = prev;
                }
            }

            void atomicAddPos(int vid, vec3 gP)
            {
                int base = vid * 3;
                atomicAddFloatBitsAtV(base+0, gP.x);
                atomicAddFloatBitsAtV(base+1, gP.y);
                atomicAddFloatBitsAtV(base+2, gP.z);
            }

            void atomicAddPoseT(vec3 gt) {
                atomicAddFloatBitsAtC(0, gt.x);
                atomicAddFloatBitsAtC(1, gt.y);
                atomicAddFloatBitsAtC(2, gt.z);
            }

            void atomicAddPoseW(vec3 gw) {
                atomicAddFloatBitsAtC(3, gw.x);
                atomicAddFloatBitsAtC(4, gw.y);
                atomicAddFloatBitsAtC(5, gw.z);
            }
                
            void atomicAddIntr(vec4 gi) {
                atomicAddFloatBitsAtC(6, gi.x);
                atomicAddFloatBitsAtC(7, gi.y);
                atomicAddFloatBitsAtC(8, gi.z);
                atomicAddFloatBitsAtC(9, gi.w);
            }

            mat3 R_wc() { return mat3(u_pose); }           // world->cam rotation
            vec3 t_wc() { return u_pose[3].xyz; }          // world->cam translation (OpenGL column-major)

            vec3 worldToCam(vec3 Pw)
            {
                return (u_pose * vec4(Pw, 1.0)).xyz;
            }

            vec2 projectPix(vec3 Pc, vec4 intr)
            {
                float invZ = 1.0 / Pc.z;
                return vec2(intr.x * (Pc.x * invZ) + intr.z,
                            intr.y * (Pc.y * invZ) + intr.w);
            }

            // Backprop through pinhole projection (pixel coords) to Pc + intr
            void backprojPix(vec3 Pc, vec4 intr, vec2 gPix, inout vec3 gPc, inout vec4 gIntrAcc)
            {
                float X = Pc.x, Y = Pc.y, Z = Pc.z;
                float fx = intr.x, fy = intr.y;
                float invZ  = 1.0 / Z;
                float invZ2 = invZ * invZ;

                // intr grads
                gIntrAcc.x += gPix.x * (X * invZ); // d x / d fx
                gIntrAcc.y += gPix.y * (Y * invZ); // d y / d fy
                gIntrAcc.z += gPix.x;              // d x / d cx
                gIntrAcc.w += gPix.y;              // d y / d cy

                // Pc grads
                gPc.x += gPix.x * (fx * invZ);
                gPc.y += gPix.y * (fy * invZ);
                gPc.z += gPix.x * (-fx * X * invZ2) + gPix.y * (-fy * Y * invZ2);
            }

            // Finite-diff texture gradient in normalized UV
            void texGradR_norm(vec2 uvNorm, out float dA_dU, out float dA_dV)
            {
                ivec2 tsI = textureSize(diffuseTex, u_texLod);
                vec2 ts = vec2(max(tsI.x,1), max(tsI.y,1));
                vec2 e = 1.0 / ts;

                float aU1 = textureLod(diffuseTex, uvNorm + vec2(e.x, 0.0), float(u_texLod)).r;
                float aU0 = textureLod(diffuseTex, uvNorm - vec2(e.x, 0.0), float(u_texLod)).r;
                float aV1 = textureLod(diffuseTex, uvNorm + vec2(0.0, e.y), float(u_texLod)).r;
                float aV0 = textureLod(diffuseTex, uvNorm - vec2(0.0, e.y), float(u_texLod)).r;

                dA_dU = (aU1 - aU0) / (2.0 * e.x);
                dA_dV = (aV1 - aV0) / (2.0 * e.y);
            }

            // Push gPc into SE3 (world->cam) + vertex world
            // Convention: local (camera-frame) perturbation:
            //   Pc' ≈ Pc + δt + δω × Pc
            void accumulateSE3AndVertex(vec3 Pw, vec3 Pc, vec3 gPc, inout vec3 gPw, inout vec3 gT, inout vec3 gW)
            {
                mat3 R = R_wc();

                // Pw: Pc = R*Pw + t -> gPw = R^T gPc
                gPw += transpose(R) * gPc;

                // t: Pc += t -> gt = gPc
                gT  += gPc;

                // ω: δPc = δω × Pc  -> gω = Pc × gPc
                gW  += cross(Pc, gPc);
            }

            void main()
            {
                ivec2 p  = ivec2(gl_GlobalInvocationID.xy);
                ivec2 sz = imageSize(rastImg);
                if (p.x >= sz.x || p.y >= sz.y) return;

                vec4 rast = imageLoad(rastImg, p);
                uint triId1 = decodeTriId(rast.w);
                if (triId1 == 0u) return;

                float u  = rast.x;
                float v  = rast.y;
                float w2 = 1.0 - u - v;

                int triIdx = int(triId1) - 1;
                int i0 = tbuf[triIdx*3 + 0];
                int i1 = tbuf[triIdx*3 + 1];
                int i2 = tbuf[triIdx*3 + 2];

                vec3 P0 = loadPos(i0);
                vec3 P1 = loadPos(i1);
                vec3 P2 = loadPos(i2);

                float g_base = imageLoad(dyImg, p).x;
                if (g_base == 0.0) return;

                vec4 intr = u_intr;

                // Interpolated world point
                vec3 P = u * P0 + v * P1 + w2 * P2;

                // Texture coordinate from projection (pixel coords)
                vec3 PcP = worldToCam(P);
                if (PcP.z <= 1e-6) return;

                vec2 texcoord = projectPix(PcP, intr);

                // sampler2D uses normalized UV
                ivec2 tsI = textureSize(diffuseTex, u_texLod);
                vec2 ts = vec2(max(tsI.x,1), max(tsI.y,1));
                vec2 uvNorm = (texcoord + vec2(0.5, 0.5)) / ts;

                float dA_dUn, dA_dVn;
                texGradR_norm(uvNorm, dA_dUn, dA_dVn);

                // dL/d(uvNorm)
                vec2 g_uvNorm = g_base * vec2(dA_dUn, dA_dVn);
                // uvNorm = (texcoord+0.5)/ts
                vec2 g_texcoord = g_uvNorm / ts;

                // Backprop projection at P
                vec3 gPcP = vec3(0.0);
                vec4 gIntrAcc = vec4(0.0);
                backprojPix(PcP, intr, g_texcoord, gPcP, gIntrAcc);

                // Accumulate grads
                vec3 gT = vec3(0.0);
                vec3 gW = vec3(0.0);

                // g wrt interpolated world point + pose
                vec3 gP_from_tex = vec3(0.0);
                accumulateSE3AndVertex(P, PcP, gPcP, gP_from_tex, gT, gW);

                // Scatter to vertices through interpolation
                atomicAddPos(i0, u  * gP_from_tex);
                atomicAddPos(i1, v  * gP_from_tex);
                atomicAddPos(i2, w2 * gP_from_tex);

                // Induce barycentric grads via P interpolation
                float g_u = dot(gP_from_tex, (P0 - P2));
                float g_v = dot(gP_from_tex, (P1 - P2));

                // ---- Barycentric path: g_u,g_v -> projected vertex pixel grads ----
                vec3 Pc0 = worldToCam(P0); if (Pc0.z <= 1e-6) return;
                vec3 Pc1 = worldToCam(P1); if (Pc1.z <= 1e-6) return;
                vec3 Pc2 = worldToCam(P2); if (Pc2.z <= 1e-6) return;

                vec2 V0p = projectPix(Pc0, intr);
                vec2 V1p = projectPix(Pc1, intr);
                vec2 V2p = projectPix(Pc2, intr);

                vec2 Ppix = vec2(p) + vec2(0.5, 0.5);

                float A  = cross2(V1p - V0p, V2p - V0p);
                if (abs(A) < 1e-12) return;

                float N0 = cross2(V1p - Ppix, V2p - Ppix); // for u
                float N1 = cross2(V2p - Ppix, V0p - Ppix); // for v

                float invA  = 1.0 / A;
                float invA2 = invA * invA;

                float g_N0 = g_u * invA;
                float g_N1 = g_v * invA;
                float g_A  = -(g_u * N0 + g_v * N1) * invA2;

                vec2 gV0p = vec2(0.0);
                vec2 gV1p = vec2(0.0);
                vec2 gV2p = vec2(0.0);

                // N0 = cross(V1-P, V2-P)
                gV1p += g_N0 *  perp(V2p - Ppix);
                gV2p += g_N0 * (-perp(V1p - Ppix));

                // N1 = cross(V2-P, V0-P)
                gV2p += g_N1 *  perp(V0p - Ppix);
                gV0p += g_N1 * (-perp(V2p - Ppix));

                // A = cross(V1-V0, V2-V0)
                vec2 dA_V1 =  perp(V2p - V0p);
                vec2 dA_V2 = -perp(V1p - V0p);
                vec2 dA_V0 = -(dA_V1 + dA_V2);

                gV0p += g_A * dA_V0;
                gV1p += g_A * dA_V1;
                gV2p += g_A * dA_V2;

                // Backprop each vertex pixel gradient through projection + SE3
                // V0
                {
                    vec3 gPc = vec3(0.0);
                    backprojPix(Pc0, intr, gV0p, gPc, gIntrAcc);

                    vec3 gPw = vec3(0.0);
                    accumulateSE3AndVertex(P0, Pc0, gPc, gPw, gT, gW);
                    atomicAddPos(i0, gPw);
                }
                // V1
                {
                    vec3 gPc = vec3(0.0);
                    backprojPix(Pc1, intr, gV1p, gPc, gIntrAcc);

                    vec3 gPw = vec3(0.0);
                    accumulateSE3AndVertex(P1, Pc1, gPc, gPw, gT, gW);
                    atomicAddPos(i1, gPw);
                }
                // V2
                {
                    vec3 gPc = vec3(0.0);
                    backprojPix(Pc2, intr, gV2p, gPc, gIntrAcc);

                    vec3 gPw = vec3(0.0);
                    accumulateSE3AndVertex(P2, Pc2, gPc, gPw, gT, gW);
                    atomicAddPos(i2, gPw);
                }

                // Commit pose + intr grads
                atomicAddPoseT(gT);
                atomicAddPoseW(gW);
                atomicAddIntr(gIntrAcc);
            }
        )Shader";

        program_ = create_program(compute_shader);

        loc_pose_ = glGetUniformLocation(program_, "u_pose");
        loc_intr_ = glGetUniformLocation(program_, "u_intr");
        loc_texLod_ = glGetUniformLocation(program_, "u_texLod");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int lvl,
                const TextureGL<Vec4<float>> &rast_texture, // RGBA32F
                const TextureGL<float> &dy_texture,         // RG32F (dy.x = dL/dbase)
                const TextureGL<float> &diffuse_texture,    // sampler2D
                BufferGL<float> &grad_vertex_pos_buffer,    // [dpx,dpy,dpz]...
                BufferGL<float> &grad_pose_intr_buffer)     // 10 floats: [dt,dw,dintr]
    {
        const int width = rast_texture.width(lvl);
        const int height = rast_texture.height(lvl);

        const int groupsX = (width + 7) / 8;
        const int groupsY = (height + 7) / 8;

        glUseProgram(program_);

        glUniformMatrix4fv(loc_pose_, 1, GL_FALSE, pose.matrix().data());
        glUniform4fv(loc_intr_, 1, cam.GetParams().data());
        glUniform1i(loc_texLod_, lvl);

        // SSBOs
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mesh.vertex_buffer_.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh.ebo_buffer_.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, grad_vertex_pos_buffer.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, grad_pose_intr_buffer.id());

        // Images
        glBindImageTexture(0, rast_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
        glBindImageTexture(1, dy_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

        // Sampler unit 0
        glBindTextureUnit(0, diffuse_texture.id());

        glDispatchCompute(groupsX, groupsY, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                        GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                        GL_TEXTURE_FETCH_BARRIER_BIT);

        glUseProgram(0);
    }

private:
    GLuint program_ = 0;
    GLint loc_pose_ = -1;
    GLint loc_intr_ = -1;
    GLint loc_texLod_ = -1;
};

class AntiAliasingFWRendererGL
{
public:
    AntiAliasingFWRendererGL()
    {
        const char *compute_shader = R"Shader(
            #version 460

            layout(local_size_x=8, local_size_y=8) in;

            // Images (2D mip level = lvl)
            layout(binding=0, r32f)    readonly  uniform image2D baseColorImg;
            layout(binding=1, rgba32f) readonly  uniform image2D rastImg;
            layout(binding=2, r32f)    writeonly uniform image2D finalColorImg;

            // Vertex buffer layout: [px,py,pz, ux,uy] * N  (stride=5 floats)
            layout(std430, binding=0) readonly buffer VertPosBuf { float vpos[]; };

            // Triangle indices: [i0,i1,i2] * T
            layout(std430, binding=1) readonly buffer TriBuf { int tri[]; };

            // Camera params
            uniform mat4 u_pose;  // world -> cam
            uniform vec4 u_intr;  // fx, fy, cx, cy

            const int VTX_STRIDE = 3; // px,py,pz

            vec3 loadPos(int vid)
            {
                int base = vid * VTX_STRIDE;
                return vec3(vpos[base+0], vpos[base+1], vpos[base+2]);
            }

            // Decode triId stored either as numeric float(triId1) or uintBitsToFloat(triId1)
            uint decodeTriId(float w)
            {
                if (w > 0.5 && w < 16777216.0) {
                    float rw = round(w);
                    if (abs(w - rw) < 1e-3) return uint(rw);
                }
                return floatBitsToUint(w);
            }

            vec2 projectPix(vec3 Pc, vec4 intr)
            {
                float invZ = 1.0 / Pc.z;
                return vec2(intr.x * (Pc.x * invZ) + intr.z,
                            intr.y * (Pc.y * invZ) + intr.w);
            }

            // Robust segment intersection for axis-aligned C0->C1 (horizontal or vertical).
            // Returns s in (0,1) along C0->C1 if AB crosses the segment.
            bool edgeCrossesBetweenCenters(vec2 A, vec2 B, vec2 C0, vec2 C1, out float s)
            {
                vec2 dC = C1 - C0;
                const float eps = 1e-8;

                // Horizontal segment
                if (abs(dC.x) > abs(dC.y)) {
                    float y0 = C0.y;
                    float dy = (B.y - A.y);
                    if (abs(dy) < eps) return false;

                    float t = (y0 - A.y) / dy; // along AB
                    if (t <= 0.0 || t >= 1.0) return false;

                    float x = A.x + t * (B.x - A.x);
                    s = (x - C0.x) / dC.x;
                    return (s > 0.0 && s < 1.0);
                }
                // Vertical segment
                else {
                    float x0 = C0.x;
                    float dx = (B.x - A.x);
                    if (abs(dx) < eps) return false;

                    float t = (x0 - A.x) / dx;
                    if (t <= 0.0 || t >= 1.0) return false;

                    float y = A.y + t * (B.y - A.y);
                    s = (y - C0.y) / dC.y;
                    return (s > 0.0 && s < 1.0);
                }
            }

            // Evaluate one edge and update best (GLSL-friendly: no lambdas)
            void evalEdge(
                vec2 A, vec2 B,
                vec2 C0, vec2 C1,
                bool horiz,
                inout float bestScore,
                inout float bestS,
                inout bool hit
            ){
                float s;
                if (!edgeCrossesBetweenCenters(A, B, C0, C1, s))
                    return;

                vec2 e = B - A;
                float ax = abs(e.x), ay = abs(e.y);
                float denom = ax + ay + 1e-12;

                // For horizontal neighbor, prefer vertical-ish edges; for vertical neighbor, prefer horizontal-ish edges.
                float score = horiz ? (ay / denom) : (ax / denom);

                if (score > bestScore) {
                    bestScore = score;
                    bestS = s;
                    hit = true;
                }
            }

            // Pick the best crossing edge of triangle in screen space for the given neighbor direction.
            bool bestCrossingEdge(vec2 V0, vec2 V1, vec2 V2, vec2 C0, vec2 C1, out float bestS)
            {
                vec2 dC = C1 - C0;
                bool horiz = (abs(dC.x) > abs(dC.y));

                float bestScore = -1.0;
                bool hit = false;
                bestS = 0.0;

                evalEdge(V0, V1, C0, C1, horiz, bestScore, bestS, hit);
                evalEdge(V1, V2, C0, C1, horiz, bestScore, bestS, hit);
                evalEdge(V2, V0, C0, C1, horiz, bestScore, bestS, hit);

                return hit;
            }

            void tryNeighbor(
                ivec2 q,
                ivec2 p,
                ivec2 sz,
                float cP,
                vec4 rP,
                uint idP,
                mat4 pose,
                vec4 intr,
                inout float bestW,
                inout float outC
            ){
                if (q.x < 0 || q.y < 0 || q.x >= sz.x || q.y >= sz.y) return;

                vec4 rQ  = imageLoad(rastImg, q);
                uint idQ = decodeTriId(rQ.w);
                if (idP == idQ) return;

                float cQ = imageLoad(baseColorImg, q).x;

                float zP = rP.z;
                float zQ = rQ.z;

                // choose front triangle id, handle background id=0
                uint frontId = 0u;
                if (idP == 0u) frontId = idQ;
                else if (idQ == 0u) frontId = idP;
                else frontId = (zP <= zQ) ? idP : idQ;

                if (frontId == 0u) return;

                int triIdx = int(frontId) - 1;
                int i0 = tri[triIdx*3 + 0];
                int i1 = tri[triIdx*3 + 1];
                int i2 = tri[triIdx*3 + 2];

                vec3 P0 = loadPos(i0);
                vec3 P1 = loadPos(i1);
                vec3 P2 = loadPos(i2);

                vec3 Pc0 = (pose * vec4(P0, 1.0)).xyz;
                vec3 Pc1 = (pose * vec4(P1, 1.0)).xyz;
                vec3 Pc2 = (pose * vec4(P2, 1.0)).xyz;

                if (Pc0.z <= 1e-6 || Pc1.z <= 1e-6 || Pc2.z <= 1e-6) return;

                vec2 V0 = projectPix(Pc0, intr);
                vec2 V1 = projectPix(Pc1, intr);
                vec2 V2 = projectPix(Pc2, intr);

                vec2 C0 = vec2(p) + vec2(0.5, 0.5);
                vec2 C1 = vec2(q) + vec2(0.5, 0.5);

                float s;
                if (!bestCrossingEdge(V0, V1, V2, C0, C1, s)) return;

                // Only adjust if crossing is in the half nearer THIS pixel
                if (s >= 0.5) return;

                float w = clamp(abs(s - 0.5), 0.0, 0.5);

                if (w > bestW) {
                    bestW = w;
                    outC  = mix(cP, cQ, w);
                }
            }

            void main()
            {
                ivec2 p  = ivec2(gl_GlobalInvocationID.xy);
                ivec2 sz = imageSize(rastImg);
                if (p.x >= sz.x || p.y >= sz.y) return;

                float cP = imageLoad(baseColorImg, p).x;
                vec4  rP = imageLoad(rastImg, p);
                uint  idP = decodeTriId(rP.w);

                float outC  = cP;
                float bestW = 0.0;

                mat4 pose = u_pose;
                vec4 intr = u_intr;

                // 4-neighborhood
                tryNeighbor(p + ivec2( 1, 0), p, sz, cP, rP, idP, pose, intr, bestW, outC);
                tryNeighbor(p + ivec2(-1, 0), p, sz, cP, rP, idP, pose, intr, bestW, outC);
                tryNeighbor(p + ivec2( 0, 1), p, sz, cP, rP, idP, pose, intr, bestW, outC);
                tryNeighbor(p + ivec2( 0,-1), p, sz, cP, rP, idP, pose, intr, bestW, outC);

                imageStore(finalColorImg, p, vec4(outC, 0.0, 0.0, 0.0));
            }
        )Shader";

        program_ = create_program(compute_shader);

        loc_pose_ = glGetUniformLocation(program_, "u_pose");
        loc_intr_ = glGetUniformLocation(program_, "u_intr");
    }

    void Render(MeshGL &mesh,
                const SE3<float> &pose_w2c,
                const PinholeCamera<float> &cam,
                int lvl,
                const TextureGL<float> &basecolor_texture,  // R32F
                const TextureGL<Vec4<float>> &rast_texture, // RGBA32F
                TextureGL<float> &final_color_texture)      // R32F
    {
        const int width  = rast_texture.width(lvl);
        const int height = rast_texture.height(lvl);

        const int groupsX = (width  + 7) / 8;
        const int groupsY = (height + 7) / 8;

        // SSBOs: vertex buffer and index buffer
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mesh.vertex_buffer_.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh.ebo_buffer_.id());

        // Images: mip level = lvl, layered MUST be GL_FALSE for normal 2D textures
        glBindImageTexture(0, basecolor_texture.id(),    lvl, GL_FALSE, 0, GL_READ_ONLY,  GL_R32F);
        glBindImageTexture(1, rast_texture.id(),         lvl, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
        glBindImageTexture(2, final_color_texture.id(),  lvl, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

        glUseProgram(program_);

        glUniformMatrix4fv(loc_pose_, 1, GL_FALSE, pose_w2c.matrix().data());
        glUniform4fv(loc_intr_, 1, cam.GetParams().data());

        glDispatchCompute(groupsX, groupsY, 1);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glUseProgram(0);
    }

private:
    GLuint program_ = 0;
    GLint  loc_pose_ = -1;
    GLint  loc_intr_ = -1;
};


class AntiAliasingBWRendererGL
{
public:
    AntiAliasingBWRendererGL()
    {
        const char *compute_shader = R"Shader(
            #version 460
            // #extension GL_EXT_shader_atomic_float : require
            // If EXT not available, try:
            // #extension GL_NV_shader_atomic_float : require

            layout(local_size_x=8, local_size_y=8) in;

            // Images
            layout(binding=0, r32f)    readonly  uniform image2D finalGradImg; // dL/d(finalColor) scalar
            layout(binding=1, r32f)    readonly  uniform image2D baseColorImg; // baseColor scalar
            layout(binding=2, rgba32f) readonly  uniform image2D rastImg;      // (u,v,depth,triIdBits or float(triId1))

            // Output for RasterShadeBW: dyImg (RG32F), dy.x = dL/dbase
            layout(binding=3, rg32f)   writeonly uniform image2D dyOutImg;

            // SSBOs
            layout(std430, binding=0) readonly buffer VertPosBuf { float vpos[]; }; // [px,py,pz] * N
            layout(std430, binding=1) readonly buffer TriBuf     { int   tri[];  }; // [i0,i1,i2] * T

            layout(std430, binding=2) buffer GradVertBuf { coherent uint gV[]; }; // [dpx,dpy,dpz] * N

            // Pose+intr grads (10 floats), SAME layout as RasterShadeBW:
            // [dtx,dty,dtz, dωx,dωy,dωz, dfx,dfy,dcx,dcy]
            layout(std430, binding=3) buffer GradPoseIntrBuf { coherent uint gC[]; };

            // Inputs
            uniform mat4 u_pose;   // world -> cam
            uniform vec4 u_intr;   // fx, fy, cx, cy

            // ---- helpers ----
            float cross2(vec2 a, vec2 b) { return a.x*b.y - a.y*b.x; }

            uint decodeTriId(float w)
            {
                // numeric float(triId1) exact up to 16,777,216
                if (w > 0.5 && w < 16777216.0) {
                    float rw = round(w);
                    if (abs(w - rw) < 1e-3) return uint(rw);
                }
                return floatBitsToUint(w);
            }

            uint triIdAt(ivec2 p) { return decodeTriId(imageLoad(rastImg, p).w); }

            vec3 loadPos(int vid)
            {
                int base = vid * 3;
                return vec3(vpos[base+0], vpos[base+1], vpos[base+2]);
            }

            float atomicAddFloatBitsAtV(int idx, float v)
            {
                uint oldBits = gVbits[idx];
                for (;;)
                {
                    float oldVal  = uintBitsToFloat(oldBits);
                    float newVal  = oldVal + v;
                    uint  newBits = floatBitsToUint(newVal);

                    uint prev = atomicCompSwap(gVbits[idx], oldBits, newBits);
                    if (prev == oldBits) return oldVal;
                    oldBits = prev;
                }
            }

            float atomicAddFloatBitsAtC(int idx, float v)
            {
                uint oldBits = gCbits[idx];
                for (;;)
                {
                    float oldVal  = uintBitsToFloat(oldBits);
                    float newVal  = oldVal + v;
                    uint  newBits = floatBitsToUint(newVal);

                    uint prev = atomicCompSwap(gCbits[idx], oldBits, newBits);
                    if (prev == oldBits) return oldVal;
                    oldBits = prev;
                }
            }

            void atomicAddPos(int vid, vec3 gP)
            {
                int base = vid * 3;
                atomicAddFloatBitsAtV(base+0, gP.x);
                atomicAddFloatBitsAtV(base+1, gP.y);
                atomicAddFloatBitsAtV(base+2, gP.z);
            }

            void atomicAddPoseT(vec3 gt) {
                atomicAddFloatBitsAtC(0, gt.x);
                atomicAddFloatBitsAtC(1, gt.y);
                atomicAddFloatBitsAtC(2, gt.z);
            }
            void atomicAddPoseW(vec3 gw) {
                atomicAddFloatBitsAtC(3, gw.x);
                atomicAddFloatBitsAtC(4, gw.y);
                atomicAddFloatBitsAtC(5, gw.z);
            }
            void atomicAddIntr(vec4 gi) {
                atomicAddFloatBitsAtC(6, gi.x);
                atomicAddFloatBitsAtC(7, gi.y);
                atomicAddFloatBitsAtC(8, gi.z);
                atomicAddFloatBitsAtC(9, gi.w);
            }

            mat3 R_wc() { return mat3(u_pose); }
            vec3 t_wc() { return u_pose[3].xyz; } // column-major translation

            vec3 worldToCam(vec3 Pw)
            {
                return (u_pose * vec4(Pw, 1.0)).xyz; // Pc = R*Pw + t
            }

            vec2 projectPix(vec3 Pc, vec4 intr)
            {
                float invZ = 1.0 / Pc.z;
                return vec2(intr.x * (Pc.x * invZ) + intr.z,
                            intr.y * (Pc.y * invZ) + intr.w);
            }

            // Backprop projection: (pixel) -> Pc + intr
            void backprojPix(vec3 Pc, vec4 intr, vec2 gPix, inout vec3 gPc, inout vec4 gIntrAcc)
            {
                float X = Pc.x, Y = Pc.y, Z = Pc.z;
                float fx = intr.x, fy = intr.y;
                float invZ  = 1.0 / Z;
                float invZ2 = invZ * invZ;

                // intr grads
                gIntrAcc.x += gPix.x * (X * invZ); // d x / d fx
                gIntrAcc.y += gPix.y * (Y * invZ); // d y / d fy
                gIntrAcc.z += gPix.x;              // d x / d cx
                gIntrAcc.w += gPix.y;              // d y / d cy

                // Pc grads
                gPc.x += gPix.x * (fx * invZ);
                gPc.y += gPix.y * (fy * invZ);
                gPc.z += gPix.x * (-fx * X * invZ2) + gPix.y * (-fy * Y * invZ2);
            }

            // SE3 local perturbation in camera frame:
            // Pc' ≈ Pc + δt + δω × Pc
            // => gt += gPc,  gω += Pc × gPc,  gPw += R^T gPc
            void accumulateSE3AndVertex(vec3 Pw, vec3 Pc, vec3 gPc, inout vec3 gPw, inout vec3 gT, inout vec3 gW)
            {
                mat3 R = R_wc();
                gPw += transpose(R) * gPc;
                gT  += gPc;
                gW  += cross(Pc, gPc);
            }

            // Segment intersection test for axis-aligned C0->C1 (4-neighborhood)
            bool edgeCrossesBetweenCenters(vec2 A, vec2 B, vec2 C0, vec2 C1, out float s)
            {
                vec2 dC = C1 - C0;
                const float eps = 1e-8;

                // horizontal neighbor
                if (abs(dC.x) > abs(dC.y)) {
                    float y0 = C0.y;
                    float dy = (B.y - A.y);
                    if (abs(dy) < eps) return false;

                    float t = (y0 - A.y) / dy;
                    if (t <= 0.0 || t >= 1.0) return false;

                    float x = A.x + t * (B.x - A.x);
                    s = (x - C0.x) / dC.x; // dC.x is ±1
                    return (s > 0.0 && s < 1.0);
                }
                // vertical neighbor
                else {
                    float x0 = C0.x;
                    float dx = (B.x - A.x);
                    if (abs(dx) < eps) return false;

                    float t = (x0 - A.x) / dx;
                    if (t <= 0.0 || t >= 1.0) return false;

                    float y = A.y + t * (B.y - A.y);
                    s = (y - C0.y) / dC.y; // dC.y is ±1
                    return (s > 0.0 && s < 1.0);
                }
            }

            // Pick best crossing edge (prefer vertical-ish for horiz neighbor, horizontal-ish for vert neighbor)
            bool bestCrossingEdge(
                vec2 V0, vec2 V1, vec2 V2,
                int i0, int i1, int i2,
                vec2 C0, vec2 C1,
                out float bestS,
                out int bestA, out int bestB,
                out vec2 bestVA, out vec2 bestVB
            ){
                vec2 dC = C1 - C0;
                bool horiz = (abs(dC.x) > abs(dC.y));
                float bestScore = -1.0;
                bool hit = false;

                // edge 0-1
                {
                    float s;
                    if (edgeCrossesBetweenCenters(V0, V1, C0, C1, s)) {
                        vec2 e = V1 - V0;
                        float ax = abs(e.x), ay = abs(e.y);
                        float denom = ax + ay + 1e-12;
                        float score = horiz ? (ay / denom) : (ax / denom);
                        if (score > bestScore) {
                            bestScore = score; bestS = s;
                            bestA = i0; bestB = i1;
                            bestVA = V0; bestVB = V1;
                            hit = true;
                        }
                    }
                }
                // edge 1-2
                {
                    float s;
                    if (edgeCrossesBetweenCenters(V1, V2, C0, C1, s)) {
                        vec2 e = V2 - V1;
                        float ax = abs(e.x), ay = abs(e.y);
                        float denom = ax + ay + 1e-12;
                        float score = horiz ? (ay / denom) : (ax / denom);
                        if (score > bestScore) {
                            bestScore = score; bestS = s;
                            bestA = i1; bestB = i2;
                            bestVA = V1; bestVB = V2;
                            hit = true;
                        }
                    }
                }
                // edge 2-0
                {
                    float s;
                    if (edgeCrossesBetweenCenters(V2, V0, C0, C1, s)) {
                        vec2 e = V0 - V2;
                        float ax = abs(e.x), ay = abs(e.y);
                        float denom = ax + ay + 1e-12;
                        float score = horiz ? (ay / denom) : (ax / denom);
                        if (score > bestScore) {
                            bestScore = score; bestS = s;
                            bestA = i2; bestB = i0;
                            bestVA = V2; bestVB = V0;
                            hit = true;
                        }
                    }
                }

                return hit;
            }

            // Test one neighbor and update "best" (inout)
            void testNeighbor(
                ivec2 p, ivec2 q, ivec2 sz,
                uint idP, float zP,
                inout float bestW,
                inout float bestS,
                inout float bestCQ,
                inout int bestVidA, inout int bestVidB,
                inout vec2 bestVA, inout vec2 bestVB,
                inout vec2 bestC0, inout vec2 bestC1
            ){
                if (q.x < 0 || q.y < 0 || q.x >= sz.x || q.y >= sz.y) return;

                vec4 rQ  = imageLoad(rastImg, q);
                uint idQ = decodeTriId(rQ.w);
                if (idP == idQ) return;

                float zQ = rQ.z;

                // Choose front triangle id, handle background
                uint frontId = 0u;
                if (idP == 0u) frontId = idQ;
                else if (idQ == 0u) frontId = idP;
                else frontId = (zP <= zQ) ? idP : idQ;

                if (frontId == 0u) return;

                int triIdx = int(frontId) - 1;
                int i0 = tri[triIdx*3 + 0];
                int i1 = tri[triIdx*3 + 1];
                int i2 = tri[triIdx*3 + 2];

                vec3 Pw0 = loadPos(i0);
                vec3 Pw1 = loadPos(i1);
                vec3 Pw2 = loadPos(i2);

                vec3 Pc0 = worldToCam(Pw0);
                vec3 Pc1 = worldToCam(Pw1);
                vec3 Pc2 = worldToCam(Pw2);

                if (Pc0.z <= 1e-6 || Pc1.z <= 1e-6 || Pc2.z <= 1e-6) return;

                vec4 intr = u_intr;
                vec2 V0 = projectPix(Pc0, intr);
                vec2 V1 = projectPix(Pc1, intr);
                vec2 V2 = projectPix(Pc2, intr);

                vec2 C0 = vec2(p) + vec2(0.5, 0.5);
                vec2 C1 = vec2(q) + vec2(0.5, 0.5);

                float s;
                int vidA, vidB;
                vec2 VA, VB;
                if (!bestCrossingEdge(V0, V1, V2, i0, i1, i2, C0, C1, s, vidA, vidB, VA, VB)) return;

                // single-writer-ish: only if crossing is in this pixel's half
                if (s >= 0.5) return;

                float w = clamp(abs(s - 0.5), 0.0, 0.5);
                if (w > bestW) {
                    bestW = w;
                    bestS = s;
                    bestCQ = imageLoad(baseColorImg, q).x;

                    bestVidA = vidA; bestVidB = vidB;
                    bestVA = VA; bestVB = VB;
                    bestC0 = C0; bestC1 = C1;
                }
            }

            void main()
            {
                ivec2 p  = ivec2(gl_GlobalInvocationID.xy);
                ivec2 sz = imageSize(rastImg);
                if (p.x >= sz.x || p.y >= sz.y) return;

                float g_out = imageLoad(finalGradImg, p).x; // scalar upstream grad
                float cP    = imageLoad(baseColorImg, p).x;

                // Default: if AA did nothing, pass through
                float g_base = g_out;

                vec4  rP  = imageLoad(rastImg, p);
                uint  idP = decodeTriId(rP.w);
                float zP  = rP.z;

                // Find best neighbor/edge (must mirror your AA forward logic)
                float bestW = 0.0;
                float bestS = 0.0;
                float bestCQ = 0.0;
                int  bestVidA = -1, bestVidB = -1;
                vec2 bestVA = vec2(0.0), bestVB = vec2(0.0);
                vec2 bestC0 = vec2(0.0), bestC1 = vec2(0.0);

                testNeighbor(p, p + ivec2( 1, 0), sz, idP, zP, bestW, bestS, bestCQ, bestVidA, bestVidB, bestVA, bestVB, bestC0, bestC1);
                testNeighbor(p, p + ivec2(-1, 0), sz, idP, zP, bestW, bestS, bestCQ, bestVidA, bestVidB, bestVA, bestVB, bestC0, bestC1);
                testNeighbor(p, p + ivec2( 0, 1), sz, idP, zP, bestW, bestS, bestCQ, bestVidA, bestVidB, bestVA, bestVB, bestC0, bestC1);
                testNeighbor(p, p + ivec2( 0,-1), sz, idP, zP, bestW, bestS, bestCQ, bestVidA, bestVidB, bestVA, bestVB, bestC0, bestC1);

                // If AA mixed this pixel in forward
                if (bestW > 0.0 && bestVidA >= 0 && bestVidB >= 0) {
                    float cQ = bestCQ;

                    // out = mix(cP, cQ, w) => dL/dcP = g_out*(1-w)
                    g_base = g_out * (1.0 - bestW);

                    // g_w = dL/dw = g_out*(cQ - cP)
                    float g_w = g_out * (cQ - cP);

                    // w = abs(s - 0.5) (clamped); for our accepted case s<0.5 => dw/ds = -1 inside unclamped region
                    float g_s = g_w * (-1.0);

                    vec2 dC = bestC1 - bestC0; // ±(1,0) or ±(0,1)

                    // Convert g_s to gradient of intersection coordinate
                    vec2 gA = vec2(0.0);
                    vec2 gB = vec2(0.0);

                    const float eps = 1e-8;

                    if (abs(dC.x) > abs(dC.y)) {
                        // s = (x_int - C0.x) / dC.x
                        float g_xint = g_s / dC.x;

                        float y0 = bestC0.y;
                        vec2 A = bestVA;
                        vec2 B = bestVB;
                        float dy = (B.y - A.y);

                        if (abs(dy) > eps) {
                            vec2 d = (B - A);
                            float f = (y0 - A.y);
                            float inv_dy = 1.0 / dy;
                            float inv_dy2 = inv_dy * inv_dy;
                            float dx = d.x;

                            // x_int = A.x + f * dx / dy
                            gA.x += g_xint * (1.0 - f * inv_dy);
                            gB.x += g_xint * (      f * inv_dy);

                            gA.y += g_xint * ( (-dx) * inv_dy + f * dx * inv_dy2 );
                            gB.y += g_xint * (                - f * dx * inv_dy2 );
                        }
                    } else {
                        // s = (y_int - C0.y) / dC.y
                        float g_yint = g_s / dC.y;

                        float x0 = bestC0.x;
                        vec2 A = bestVA;
                        vec2 B = bestVB;
                        float dx = (B.x - A.x);

                        if (abs(dx) > eps) {
                            vec2 d = (B - A);
                            float f = (x0 - A.x);
                            float inv_dx = 1.0 / dx;
                            float inv_dx2 = inv_dx * inv_dx;
                            float dy = d.y;

                            // y_int = A.y + f * dy / dx
                            gA.y += g_yint * (1.0 - f * inv_dx);
                            gB.y += g_yint * (      f * inv_dx);

                            gA.x += g_yint * ( (-dy) * inv_dx + f * dy * inv_dx2 );
                            gB.x += g_yint * (                - f * dy * inv_dx2 );
                        }
                    }

                    // Push endpoint grads to Pc, then to Pw + SE3 + intr
                    vec4 gIntrAcc = vec4(0.0);
                    vec3 gT = vec3(0.0);
                    vec3 gW = vec3(0.0);

                    // Endpoint A
                    {
                        int vid = bestVidA;
                        vec3 Pw = loadPos(vid);
                        vec3 Pc = worldToCam(Pw);

                        vec3 gPc = vec3(0.0);
                        backprojPix(Pc, u_intr, gA, gPc, gIntrAcc);

                        vec3 gPw = vec3(0.0);
                        accumulateSE3AndVertex(Pw, Pc, gPc, gPw, gT, gW);
                        atomicAddPos(vid, gPw);
                    }

                    // Endpoint B
                    {
                        int vid = bestVidB;
                        vec3 Pw = loadPos(vid);
                        vec3 Pc = worldToCam(Pw);

                        vec3 gPc = vec3(0.0);
                        backprojPix(Pc, u_intr, gB, gPc, gIntrAcc);

                        vec3 gPw = vec3(0.0);
                        accumulateSE3AndVertex(Pw, Pc, gPc, gPw, gT, gW);
                        atomicAddPos(vid, gPw);
                    }

                    atomicAddPoseT(gT);
                    atomicAddPoseW(gW);
                    atomicAddIntr(gIntrAcc);
                }

                // OUTPUT dy for RasterShadeBW: dy.x = dL/dbase
                imageStore(dyOutImg, p, vec4(g_base, 0.0, 0.0, 0.0));
            }
        )Shader";

        program_ = create_program(compute_shader);

        loc_pose_ = glGetUniformLocation(program_, "u_pose");
        loc_intr_ = glGetUniformLocation(program_, "u_intr");
    }

    void Render(MeshGL &mesh,
                SE3<float> pose,
                PinholeCamera<float> cam,
                int lvl,
                const TextureGL<float> &final_grad_texture, // R32F
                const TextureGL<float> &basecolor_texture,  // R32F
                const TextureGL<Vec4<float>> &rast_texture, // RGBA32F
                TextureGL<float> &dy_out_texture,           // R32F: -> RasterShadeBW dyImg
                BufferGL<float> &grad_pos_buffer,           // [dpx,dpy,dpz]...
                BufferGL<float> &grad_pose_intr_buffer     // 10 floats: [dt,dw,dintr]
    )
    {
        const int width = rast_texture.width(lvl);
        const int height = rast_texture.height(lvl);

        // local_size = 8x8
        const int groupsX = (width + 7) / 8;
        const int groupsY = (height + 7) / 8;

        glUseProgram(program_);

        glUniformMatrix4fv(loc_pose_, 1, GL_FALSE, pose.matrix().data());
        glUniform4fv(loc_intr_, 1, cam.GetParams().data());

        // SSBOs
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mesh.vertex_buffer_.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh.ebo_buffer_.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, grad_pos_buffer.id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, grad_pose_intr_buffer.id());

        // Images
        glBindImageTexture(0, final_grad_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
        glBindImageTexture(1, basecolor_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
        glBindImageTexture(2, rast_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
        glBindImageTexture(3, dy_out_texture.id(), lvl, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

        glDispatchCompute(groupsX, groupsY, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                        GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        glUseProgram(0);
    }

private:
    GLuint program_ = 0;
    GLint loc_pose_ = -1;
    GLint loc_intr_ = -1;
};
