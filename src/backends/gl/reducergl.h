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

    void Render(int lvl,
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