#pragma once
// #include <string>
// #include <vector>
// #include <utility>
// #include <iostream>
// #include <type_traits>
// #include <cassert>

#include "mpdr/common/render_constants.h"
#include "mpdr/common/error_handling.h"
#include "mpdr/common/types.h"
#include "mpdr/common/camera.h"
#include "mpdr/common/boundingbox.h"
#include "mpdr/backends/gl/common.h"
#include "mpdr/backends/gl/meshgl.h"
#include "mpdr/backends/gl/texturegl.h"
#include "mpdr/backends/gl/renderergl.h"

class ResidualReduceGL
{
public:
    ResidualReduceGL()
    {
        const char *compute_shader = R"Shader(
        #version 460

        layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

        // Use the correct layout() for your textures.
        // rgba8 works for GL_RGBA8; rgba16f for GL_RGBA16F, etc.
        //layout(binding = 0, rgb32f)  readonly uniform image2D kf_pos_texture;
        layout(binding = 0) uniform sampler2D kf_pos_texture;
        layout(binding = 1) uniform sampler2D kf_texture;
        layout(binding = 2) uniform sampler2D f_texture;
        //layout(binding = 2, r32f)  readonly uniform image2D f_texture;

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
    
        uniform vec3 kf_pos_nodata;
        uniform float kf_nodata;
        uniform float f_nodata;
        uniform vec4 cam;
        uniform int lvl;

        shared float sdata[16 * 16];
        shared int sdata_valid[16 * 16];

        void main()
        {
            ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
            uint  lid = gl_LocalInvocationIndex; // 0..255 within the workgroup

            if (gid.x >= uSize.x || gid.y >= uSize.y)
                return;
            
            //vec3 kf_pos = imageLoad(kf_pos_texture, gid).xyz;
            vec3 kf_pos = texelFetch(kf_pos_texture, gid, lvl).rgb;

            if(kf_pos.z <= 0.0f)
                return;

            vec2 texcoord = pointToPix(kf_pos, cam.x, cam.y, cam.z, cam.w);

            if(texcoord.x < 0.0 || texcoord.x > 1.0 || texcoord.y < 0.0 || texcoord.y > 1.0)
                return;

            float kf = textureLod(kf_texture, texcoord, float(lvl)).x;
            //float kf = imageLoad(kf_texture, gid).x;
            //float f = imageLoad(f_texture, gid).x;
            float f = texelFetch(f_texture, gid, lvl).r;

            if (kf == kf_nodata || f == f_nodata)
                return;
            
            float v = (kf - f) * (kf - f);
            int valid = 1;
        
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

        computeProgram_ = create_program(compute_shader, common);
        glGenBuffers(1, &error_ssbo_);
        glGenBuffers(1, &count_ssbo_);
    }

    float compute(const PinholeCamera<float> &cam,
                  int lvl,
                  const TextureGL<Vec3<float>> gbuf_kfpos,
                  const TextureGL<ImageType> &kf_texture,
                  const TextureGL<ImageType> &f_texture,
                  float huber_thresh)
    {
        int width = kf_texture.width(lvl);
        int height = kf_texture.height(lvl);

        int groupsX = (width + 15) / 16;
        int groupsY = (height + 15) / 16;
        int numGroups = groupsX * groupsY;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, error_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numGroups * sizeof(float), nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, error_ssbo_);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, count_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, numGroups * sizeof(int), nullptr, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, count_ssbo_);

        Vec3<float> kf_pos_nodata = gbuf_kfpos.nodata();
        float kf_nodata = kf_texture.nodata();
        float f_nodata = f_texture.nodata();

        // glBindImageTexture(0, gbuf_kfpos.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_RGB32F);
        // glBindImageTexture(1, kf_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
        // glBindImageTexture(2, f_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gbuf_kfpos.id());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, kf_texture.id());
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, f_texture.id());

        Vec4<float> cam_params = cam.GetParams();

        glUseProgram(computeProgram_);
        glUniform2i(glGetUniformLocation(computeProgram_, "uSize"), width, height);
        glUniform1i(glGetUniformLocation(computeProgram_, "uNumGroupsX"), groupsX);
        glUniform4f(glGetUniformLocation(computeProgram_, "cam"), cam_params(0), cam_params(1), cam_params(2), cam_params(3));
        glUniform1i(glGetUniformLocation(computeProgram_, "lvl"), lvl);
        glUniform3f(glGetUniformLocation(computeProgram_, "kf_pos_nodata"), kf_pos_nodata(0), kf_pos_nodata(1), kf_pos_nodata(2));
        glUniform1f(glGetUniformLocation(computeProgram_, "kf_nodata"), kf_nodata);
        glUniform1f(glGetUniformLocation(computeProgram_, "f_nodata"), f_nodata);

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

        return std::sqrt(sum / total_count);
    }

private:
    GLuint computeProgram_;
    GLuint error_ssbo_;
    GLuint count_ssbo_;
};

class ResidualRRGL
{
public:
    ResidualRRGL()
        : fpos_texture(1, 1, Vec3<float>(0.0, 0.0, 0.0)),
          kfpos_texture(1, 1, Vec3<float>(0.0, 0.0, 0.0)),
          bc_texture(1, 1, Vec3<float>(0.0, 0.0, 0.0))
    {
    }

    float compute(const MeshGL &mesh,
                  const SE3<float> &pose,
                  const PinholeCamera<float> &cam,
                  int lvl,
                  const TextureGL<ImageType> &kf_texture,
                  const TextureGL<ImageType> &f_texture,
                  float huber_thresh)
    {
        if (fpos_texture.width(0) != kf_texture.width(0) || fpos_texture.height(0) != kf_texture.height(0))
        {
            fpos_texture = TextureGL<Vec3<float>>(kf_texture.width(0), kf_texture.height(0), Vec3<float>(0.0, 0.0, 0.0));
            kfpos_texture = TextureGL<Vec3<float>>(kf_texture.width(0), kf_texture.height(0), Vec3<float>(0.0, 0.0, 0.0));
            bc_texture = TextureGL<Vec3<float>>(kf_texture.width(0), kf_texture.height(0), Vec3<float>(0.0, 0.0, 0.0));
        }
        referred_renderer.Render(mesh, pose, cam, lvl, fpos_texture, kfpos_texture, bc_texture);
        return residual_reduce.compute(cam, lvl, kfpos_texture, kf_texture, f_texture, huber_thresh);

        /*
        auto kfpos_map = kfpos_texture.MapRead(lvl);
        auto kf_map = kf_texture.MapRead(lvl);
        auto f_map = f_texture.MapRead(lvl);

        float sum = 0.0;
        int count = 0;
        for (int y = 0; y < kfpos_texture.height(lvl); y++)
        {
            for (int x = 0; x < kfpos_texture.width(lvl); x++)
            {
                Vec3<float> kfpos = kfpos_map(y, x);
                if (kfpos(2) <= 0.0f)
                    continue;

                Vec2<float> texcoord = cam.pointToPix(kfpos);
                if (texcoord(0) < 0.0 || texcoord(0) > 1.0 || texcoord(1) < 0.0 || texcoord(1) > 1.0)
                    continue;

                float kf = sample<float, TextureViewReadGL<float>>(kf_map, texcoord(1), texcoord(0));
                float f = f_map(y, x);
                if (kf == kf_texture.nodata() || f == f_texture.nodata())
                    continue;

                float diff = f - kf;
                float w = huber_weight(diff, huber_thresh);
                sum += w * diff * diff;
                count++;
            }
        }
        return std::sqrt(sum / count);
        */
    }

private:
    DeferredRendererGL referred_renderer;
    ResidualReduceGL residual_reduce;

    TextureGL<Vec3<float>> fpos_texture;
    TextureGL<Vec3<float>> kfpos_texture;
    TextureGL<Vec3<float>> bc_texture;
};