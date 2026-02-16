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
#include "backends/gl/common.h"
#include "backends/gl/meshgl.h"
#include "backends/gl/texturegl.h"

class DepthDeferredRendererGL
{
public:
    DepthDeferredRendererGL()
    {
        const char *compute_shader = R"Shader(
        // aa_fwd.comp
        #version 430

        layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

        // Use the correct layout() for your textures.
        // rgba8 works for GL_RGBA8; rgba16f for GL_RGBA16F, etc.
        layout(binding = 0, rgb32f)  readonly uniform image2D kf_pos_texture;
        layout(binding = 1, r32f)  readonly uniform sampler2D kf_texture;
        layout(binding = 2, r32f)  readonly uniform image2D f_texture;

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
                vec3 kf_pos = imageLoad(kf_texture, gid).xyz;
                vec2 texcoord = pointToPix(kf_pos, cam.x, cam.y, cam.z, cam.w);
                float kf = texture(image, texcoord).x;
                //float kf = imageLoad(kf_texture, gid).x;
                float f = imageLoad(f_texture, gid).x;
                
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

        computeProgram_ = create_program(compute_shader, common);
        glGenBuffers(1, &error_ssbo_);
        glGenBuffers(1, &count_ssbo_);
    }

    float compute(const MeshGL &mesh,
                  const SE3<float> &pose,
                  const PinholeCamera<float> &cam,
                  int lvl,
                  const TextureGL<Vec3<float>> gbuf_fpos;
                  const TextureGL<Vec3<float>> gbuf_kfpos;
                  const TextureGL<Vec3<float>> gbuf_bcid;
                  TextureGL<ImageType> &depth_texture)
    {
        int width = depth_texture.width(lvl);
        int height = depth_texture.height(lvl);

        int groupsX = (width + 15) / 16;
        int groupsY = (height + 15) / 16;
        int numGroups = groupsX * groupsY;

        float kf_nodata = kf_texture.nodata();
        float f_nodata = f_texture.nodata();

        glBindImageTexture(0, gbuf_kfpos.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_RGB32F);
        glBindImageTexture(1, depth_texture.id(), lvl, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

        glUseProgram(computeProgram_);
        glUniform2i(glGetUniformLocation(computeProgram_, "uSize"), width, height);
        glUniform1i(glGetUniformLocation(computeProgram_, "uNumGroupsX"), groupsX);
        glUniform4v(glGetUniformLocation(computeProgram_, "cam"), cam.fx, cam.fy, cam.cx, cam.cy);

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



