#pragma once

#include "backends/gl/common.h"

class ResidualReducerGL 
{
public:
    ResidualReducerGL()
    {
        const char *compute_shader = R"Shader(
        // aa_fwd.comp
        #version 430

        layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

        // Use the correct layout() for your textures.
        // rgba8 works for GL_RGBA8; rgba16f for GL_RGBA16F, etc.
        layout(binding = 0, r32f)  readonly uniform image2D imgA;
        layout(binding = 1, r32f)  readonly uniform image2D imgB;

        layout(std430, binding = 0) buffer PartialSums
        {
            float sums[]; // one entry per workgroup
        };

        layout(std430, binding = 1) buffer PartialCounts
        {
            int counts[]; // one entry per workgroup
        };

        uniform ivec2 uSize;        // image size in pixels
        uniform int   uNumGroupsX;  // dispatch groups in X (ceil(width/16))

        uniform float imgA_nodata;
        uniform float imgB_nodata;

        shared float sdata[16 * 16];
        shared int sdata_valid[16 * 16];

        void main()
        {
            ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
            uint  lid = gl_LocalInvocationIndex; // 0..255 within the workgroup

            float v = 0.0;
            int valid = 0;

            if (gid.x < uSize.x && gid.y < uSize.y)
            {
                float a = imageLoad(imgA, gid).x;
                float b = imageLoad(imgB, gid).x;
                
                if (a != imgA_nodata && b != imgB_nodata)
                {
                    v = (a - b) * (a - b);
                    valid = 1;
                }
            }

            sdata[lid] = v;
            sdata_valid[lid] = valid;
            barrier();

            // Parallel reduction in shared memory (256 -> 1)
            for (uint stride = (16u * 16u) / 2u; stride > 0u; stride >>= 1u)
            {
                if (lid < stride)
                {
                    sdata[lid] += sdata[lid + stride];
                    sdata_valid[lid] += sdata_valid[lid + stride];
                }
                barrier();
            }

            // One thread writes the workgroup sum
            if (lid == 0u)
            {
                uint groupIndex = uint(gl_WorkGroupID.x) + uint(gl_WorkGroupID.y) * uint(uNumGroupsX);
                sums[groupIndex] = sdata[0];
                counts[groupIndex] = sdata_valid[0];
            }
        }
        )Shader";

        computeProgram_ = create_program(compute_shader);
        glGenBuffers(1, &error_ssbo_);
        glGenBuffers(1, &count_ssbo_);
    }

    void reduce(int lvl,
                const TextureGL<ImageType> &image_1_texture,
                const TextureGL<ImageType> &image_2_texture,
                float &error)
    {
        //save_state();

        int width  = image_1_texture.width(lvl);
        int height = image_1_texture.height(lvl);

        int groupsX = (width  + 15) / 16;
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

        glBindImageTexture(0, image_1_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY,  GL_R32F);
        glBindImageTexture(1, image_2_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY,  GL_R32F);

        glUseProgram(computeProgram_);
        glUniform2i(glGetUniformLocation(computeProgram_, "uSize"), width, height);
        glUniform1i(glGetUniformLocation(computeProgram_, "uNumGroupsX"), groupsX);

        glDispatchCompute(groupsX, groupsY, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, error_ssbo_);
        float* partial_sum = (float*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                                numGroups * sizeof(float),
                                                GL_MAP_READ_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_ssbo_);
        int* partial_count = (int*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                                numGroups * sizeof(int),
                                                GL_MAP_READ_BIT);

        double sum = 0.0;
        int total_count = 0;
        for (int i = 0; i < numGroups; i++) {
            sum += (double)partial_sum[i];
            total_count += partial_count[i];
        }

        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        error = std::sqrt(sum / total_count);
    }

private:
    GLuint computeProgram_;
    GLuint error_ssbo_;
    GLuint count_ssbo_;
};

// Reduces normal equations for a 6DOF pose:
//   H += Σ w * J * J^T   (6x6 symmetric)
//   g += Σ w * J * r     (6)
//   cost += Σ w * r^2    (optional)
//   count += Σ 1(valid)
//
// Keeps the same overall structure as your ResidualReducerGL:
//   - per-workgroup reduction
//   - write one entry per workgroup into SSBO
//   - map SSBO on CPU and finalize reduction
//
/*
class HGPoseReducerGL
{
public:
    HGPoseReducerGL()
    {
        const char *compute_shader = R"Shader(
        #version 430

        layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

        // Use samplers + texelFetch to avoid image-format constraints (RGB32F etc).
        // Bindings are fixed via layout(binding=...).
        layout(binding = 0) uniform sampler2D uImg;
        layout(binding = 1) uniform sampler2D uRef;
        layout(binding = 2) uniform sampler2D uJtra; // Vec3 in .xyz
        layout(binding = 3) uniform sampler2D uJrot; // Vec3 in .xyz

        // We store 28 floats per workgroup as 7 vec4s:
        //   21 Hessian upper-tri
        //    6 gradient
        //    1 cost (optional)
        //
        // hg4 has length = numGroups * 7
        layout(std430, binding = 0) buffer PartialHG
        {
            vec4 hg4[];
        };

        layout(std430, binding = 1) buffer PartialCounts
        {
            int counts[];
        };

        uniform ivec2 uSize;        // image size
        uniform int   uNumGroupsX;  // dispatch groups in X
        uniform int   uLevel;       // mip/level (for texelFetch)

        uniform float uNoDataImg;
        uniform float uNoDataRef;
        uniform vec3  uNoDataJtra;
        uniform vec3  uNoDataJrot;

        uniform float uHuberThresh;

        // Shared: 7 vec4 accumulators per thread
        shared vec4 sHG[7 * 256];
        shared int  sCount[256];

        // NOTE: If your nodata is NaN, "v == nod" will NEVER be true.
        // In that case, use isnan(v) checks instead.
        bool is_nodata_scalar(float v, float nod) { return v == nod; }
        bool is_nodata_vec3(vec3 v, vec3 nod) { return all(equal(v, nod)); }

        float huber_weight(float r, float t)
        {
            float a = abs(r);
            // t <= 0 => no robustification
            if (t <= 0.0) return 1.0;
            return (a <= t) ? 1.0 : (t / a);
        }

        void main()
        {
            ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
            uint  lid = gl_LocalInvocationIndex; // 0..255

            vec4 v0 = vec4(0.0);
            vec4 v1 = vec4(0.0);
            vec4 v2 = vec4(0.0);
            vec4 v3 = vec4(0.0);
            vec4 v4 = vec4(0.0);
            vec4 v5 = vec4(0.0);
            vec4 v6 = vec4(0.0);
            int valid = 0;

            if (gid.x < uSize.x && gid.y < uSize.y)
            {
                float img = texelFetch(uImg, gid, uLevel).x;
                float ref = texelFetch(uRef, gid, uLevel).x;

                vec3 jt = texelFetch(uJtra, gid, uLevel).xyz;
                vec3 jr = texelFetch(uJrot, gid, uLevel).xyz;

                if (!is_nodata_scalar(img, uNoDataImg) &&
                    !is_nodata_scalar(ref, uNoDataRef) &&
                    !is_nodata_vec3(jt, uNoDataJtra) &&
                    !is_nodata_vec3(jr, uNoDataJrot))
                {
                    float r = (ref - img);
                    float w = huber_weight(r, uHuberThresh);

                    // J = [jt.x jt.y jt.z jr.x jr.y jr.z]
                    float J0 = jt.x;
                    float J1 = jt.y;
                    float J2 = jt.z;
                    float J3 = jr.x;
                    float J4 = jr.y;
                    float J5 = jr.z;

                    // Upper triangle of H = w * J * J^T
                    float h00 = w * J0 * J0;
                    float h01 = w * J0 * J1;
                    float h11 = w * J1 * J1;
                    float h02 = w * J0 * J2;

                    float h12 = w * J1 * J2;
                    float h22 = w * J2 * J2;
                    float h03 = w * J0 * J3;
                    float h13 = w * J1 * J3;

                    float h23 = w * J2 * J3;
                    float h33 = w * J3 * J3;
                    float h04 = w * J0 * J4;
                    float h14 = w * J1 * J4;

                    float h24 = w * J2 * J4;
                    float h34 = w * J3 * J4;
                    float h44 = w * J4 * J4;
                    float h05 = w * J0 * J5;

                    float h15 = w * J1 * J5;
                    float h25 = w * J2 * J5;
                    float h35 = w * J3 * J5;
                    float h45 = w * J4 * J5;

                    float h55 = w * J5 * J5;

                    // Gradient g = w * J * r
                    float g0 = w * J0 * r;
                    float g1 = w * J1 * r;
                    float g2 = w * J2 * r;
                    float g3 = w * J3 * r;
                    float g4 = w * J4 * r;
                    float g5 = w * J5 * r;

                    // Optional cost term (handy sanity check)
                    float c = w * r * r;

                    // Pack into 7 vec4s (28 floats)
                    // Indices 0..20: Hessian upper-tri
                    // 21..26: gradient
                    // 27: cost
                    v0 = vec4(h00, h01, h11, h02);      // 0..3
                    v1 = vec4(h12, h22, h03, h13);      // 4..7
                    v2 = vec4(h23, h33, h04, h14);      // 8..11
                    v3 = vec4(h24, h34, h44, h05);      // 12..15
                    v4 = vec4(h15, h25, h35, h45);      // 16..19
                    v5 = vec4(h55, g0,  g1,  g2);       // 20..23
                    v6 = vec4(g3,  g4,  g5,  c);        // 24..27

                    valid = 1;
                }
            }

            // Store to shared
            sHG[0 * 256 + lid] = v0;
            sHG[1 * 256 + lid] = v1;
            sHG[2 * 256 + lid] = v2;
            sHG[3 * 256 + lid] = v3;
            sHG[4 * 256 + lid] = v4;
            sHG[5 * 256 + lid] = v5;
            sHG[6 * 256 + lid] = v6;
            sCount[lid] = valid;

            barrier();

            // Parallel reduce (256 -> 1)
            for (uint stride = 128u; stride > 0u; stride >>= 1u)
            {
                if (lid < stride)
                {
                    sHG[0 * 256 + lid] += sHG[0 * 256 + lid + stride];
                    sHG[1 * 256 + lid] += sHG[1 * 256 + lid + stride];
                    sHG[2 * 256 + lid] += sHG[2 * 256 + lid + stride];
                    sHG[3 * 256 + lid] += sHG[3 * 256 + lid + stride];
                    sHG[4 * 256 + lid] += sHG[4 * 256 + lid + stride];
                    sHG[5 * 256 + lid] += sHG[5 * 256 + lid + stride];
                    sHG[6 * 256 + lid] += sHG[6 * 256 + lid + stride];

                    sCount[lid] += sCount[lid + stride];
                }
                barrier();
            }

            if (lid == 0u)
            {
                uint groupIndex = uint(gl_WorkGroupID.x) + uint(gl_WorkGroupID.y) * uint(uNumGroupsX);
                uint base = groupIndex * 7u;

                hg4[base + 0u] = sHG[0 * 256];
                hg4[base + 1u] = sHG[1 * 256];
                hg4[base + 2u] = sHG[2 * 256];
                hg4[base + 3u] = sHG[3 * 256];
                hg4[base + 4u] = sHG[4 * 256];
                hg4[base + 5u] = sHG[5 * 256];
                hg4[base + 6u] = sHG[6 * 256];

                counts[groupIndex] = sCount[0];
            }
        }
        )Shader";

        computeProgram_ = create_program(compute_shader);
        glGenBuffers(1, &hg_ssbo_);
        glGenBuffers(1, &count_ssbo_);
    }

    // Same usage style as your CPU reducer: total += compute(...)
    void reduce(int lvl,
                const TextureGL<Vec3<float>> &jtra_texture,
                const TextureGL<Vec3<float>> &jrot_texture,
                const TextureGL<ImageType> &image_texture,
                const TextureGL<ImageType> &ref_texture,
                DenseLinearProblem<6> &total,
                float huber_thresh)
    {
        total += compute(lvl, jtra_texture, jrot_texture, image_texture, ref_texture, huber_thresh);
    }

    [[nodiscard]] DenseLinearProblem<6> compute(int lvl,
                                                const TextureGL<Vec3f> &jtra_texture,
                                                const TextureGL<Vec3f> &jrot_texture,
                                                const TextureGL<ImageType> &image_texture,
                                                const TextureGL<ImageType> &ref_texture,
                                                float huber_thresh)
    {
        const int width  = image_texture.width(lvl);
        const int height = image_texture.height(lvl);

        const int groupsX = (width  + 15) / 16;
        const int groupsY = (height + 15) / 16;
        const int numGroups = groupsX * groupsY;

        // 7 vec4 per group => 28 floats per group
        const GLsizeiptr hg_bytes = GLsizeiptr(numGroups) * 7 * sizeof(float) * 4;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, hg_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, hg_bytes, nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, hg_ssbo_);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, GLsizeiptr(numGroups) * sizeof(int), nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, count_ssbo_);

        // Bind textures as samplers (layout(binding=0..3) in shader)
        // Using classic binding to avoid requiring GL 4.5 glBindTextureUnit.
        glUseProgram(computeProgram_);

        auto bindSampler2D = [&](int unit, GLuint texId, const char* uniformName)
        {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, texId);
            GLint loc = glGetUniformLocation(computeProgram_, uniformName);
            glUniform1i(loc, unit);
        };

        bindSampler2D(0, image_texture.id(), "uImg");
        bindSampler2D(1, ref_texture.id(),   "uRef");
        bindSampler2D(2, jtra_texture.id(),  "uJtra");
        bindSampler2D(3, jrot_texture.id(),  "uJrot");

        glUniform2i(glGetUniformLocation(computeProgram_, "uSize"), width, height);
        glUniform1i(glGetUniformLocation(computeProgram_, "uNumGroupsX"), groupsX);
        glUniform1i(glGetUniformLocation(computeProgram_, "uLevel"), lvl);

        // nodata uniforms
        glUniform1f(glGetUniformLocation(computeProgram_, "uNoDataImg"), float(image_texture.nodata()));
        glUniform1f(glGetUniformLocation(computeProgram_, "uNoDataRef"), float(ref_texture.nodata()));

        const Vec3f nd_jt = jtra_texture.nodata();
        const Vec3f nd_jr = jrot_texture.nodata();
        glUniform3f(glGetUniformLocation(computeProgram_, "uNoDataJtra"), nd_jt(0), nd_jt(1), nd_jt(2));
        glUniform3f(glGetUniformLocation(computeProgram_, "uNoDataJrot"), nd_jr(0), nd_jr(1), nd_jr(2));

        glUniform1f(glGetUniformLocation(computeProgram_, "uHuberThresh"), huber_thresh);

        glDispatchCompute(groupsX, groupsY, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Map partials
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, hg_ssbo_);
        float *partial_hg = (float*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, hg_bytes, GL_MAP_READ_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_ssbo_);
        int *partial_count = (int*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                                    GLsizeiptr(numGroups) * sizeof(int),
                                                    GL_MAP_READ_BIT);

        // CPU finalize
        // We'll accumulate in double for stability.
        double H[6][6] = {};
        double g[6]    = {};
        double cost    = 0.0;
        int total_count = 0;

        auto add_upper = [&](double h00,double h01,double h11,double h02,double h12,double h22,
                             double h03,double h13,double h23,double h33,
                             double h04,double h14,double h24,double h34,double h44,
                             double h05,double h15,double h25,double h35,double h45,double h55)
        {
            // Fill symmetric H from upper-tri pieces
            H[0][0] += h00;
            H[0][1] += h01; H[1][0] += h01;
            H[1][1] += h11;
            H[0][2] += h02; H[2][0] += h02;
            H[1][2] += h12; H[2][1] += h12;
            H[2][2] += h22;

            H[0][3] += h03; H[3][0] += h03;
            H[1][3] += h13; H[3][1] += h13;
            H[2][3] += h23; H[3][2] += h23;
            H[3][3] += h33;

            H[0][4] += h04; H[4][0] += h04;
            H[1][4] += h14; H[4][1] += h14;
            H[2][4] += h24; H[4][2] += h24;
            H[3][4] += h34; H[4][3] += h34;
            H[4][4] += h44;

            H[0][5] += h05; H[5][0] += h05;
            H[1][5] += h15; H[5][1] += h15;
            H[2][5] += h25; H[5][2] += h25;
            H[3][5] += h35; H[5][3] += h35;
            H[4][5] += h45; H[5][4] += h45;
            H[5][5] += h55;
        };

        for (int i = 0; i < numGroups; ++i)
        {
            const float *p = partial_hg + i * 28;

            // Unpack (same order as shader packing)
            const double h00 = p[0],  h01 = p[1],  h11 = p[2],  h02 = p[3];
            const double h12 = p[4],  h22 = p[5],  h03 = p[6],  h13 = p[7];
            const double h23 = p[8],  h33 = p[9],  h04 = p[10], h14 = p[11];
            const double h24 = p[12], h34 = p[13], h44 = p[14], h05 = p[15];
            const double h15 = p[16], h25 = p[17], h35 = p[18], h45 = p[19];
            const double h55 = p[20];

            const double g0 = p[21], g1 = p[22], g2 = p[23];
            const double g3 = p[24], g4 = p[25], g5 = p[26];

            const double c  = p[27];

            add_upper(h00,h01,h11,h02,h12,h22,h03,h13,h23,h33,h04,h14,h24,h34,h44,h05,h15,h25,h35,h45,h55);

            g[0] += g0; g[1] += g1; g[2] += g2;
            g[3] += g3; g[4] += g4; g[5] += g5;

            cost += c;
            total_count += partial_count[i];
        }

        // Unmap correctly (bind each before unmapping)
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, hg_ssbo_);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_ssbo_);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        DenseLinearProblem<6> out;

        // ---- Adapt these lines to your DenseLinearProblem<6> layout ----
        // Typical possibilities:
        //   out.H(i,j) or out.A(i,j)  (Eigen style)
        //   out.b(i) or out.g(i)
        //   out.cost / out.error, out.count
        //
        // Fill Hessian:
        for (int r = 0; r < 6; ++r)
            for (int c = 0; c < 6; ++c)
                out.H(r, c) += float(H[r][c]);

        // Fill gradient:
        for (int r = 0; r < 6; ++r)
            out.g(r) += float(g[r]);

        out.count += total_count;
        out.cost  += float(cost);
        // ---------------------------------------------------------------

        return out;
    }

private:
    GLuint computeProgram_ = 0;
    GLuint hg_ssbo_ = 0;     // numGroups * 7 vec4
    GLuint count_ssbo_ = 0;  // numGroups * int
};
*/