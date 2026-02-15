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
#include "backends/gl/deferredrenderergl.h"

class ResidualRRGL
{
public:
    ResidualRRGL()
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
                  int out_lvl,
                  const TextureGL<Vec3<float>> gbuf_fpos;
                  const TextureGL<Vec3<float>> gbuf_kfpos;
                  const TextureGL<Vec3<float>> gbuf_bcid;
                  const TextureGL<ImageType> &kf_texture,
                  const TextureGL<ImageType> &f_texture)
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

        float kf_nodata = kf_texture.nodata();
        float f_nodata = f_texture.nodata();

        glBindImageTexture(0, gbuf_kfpos.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_RGB32F);
        glBindImageTexture(1, kf_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);
        glBindImageTexture(2, f_texture.id(), lvl, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

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

class JPoseExpRendererGL
{
public:
    JPoseExpRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core

            layout (location = 0) in vec3 a_position;
            
            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec3 f_ver;
            out vec3 kf_ver;

            void main() {
                vec4 ver = pose_matrix * vec4(a_position, 1.0);
                gl_Position = view_matrix * ver;
                f_ver = ver.xyz;
                kf_ver = a_position;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core

            layout(location = 0) out vec3 jtra_output;
            layout(location = 1) out vec3 jrot_output;
            layout(location = 2) out vec3 jexp_output;
            layout(location = 3) out float image_output;

            in vec3 kf_ver;
            in vec3 f_ver;

            uniform sampler2D kf_image;
            uniform float kf_image_nodata;

            uniform sampler2D dfdxy_image;
            uniform vec3 dfdxy_image_nodata;
            
            uniform int in_lvl;
            uniform int out_lvl;

            uniform float fx;
            uniform float fy;
            uniform float cx;
            uniform float cy;

            uniform int out_width;
            uniform int out_height;

            uniform vec2 exposure;

            void main()
            {
                vec2 texcoord = pointToPix(kf_ver, fx, fy, cx, cy);
                if(texcoord.x < 0.0 || texcoord.x > 1.0 || texcoord.y < 0.0 || texcoord.y > 1.0)
                {
                    discard;
                }

                float kf = textureLod(kf_image, texcoord, float(in_lvl)).r;
                // vec2 d_f_d_xy = texelFetch(dfdxy_image, ivec2(gl_FragCoord.x, gl_FragCoord.y), out_lvl).xy;
                vec3 d_f_d_xy = textureLod(dfdxy_image, texcoord, float(in_lvl)).xyz;

                //if (kf == kf_image_nodata || f == f_image_nodata || dfdxy.xy == dfdxy_image_nodata.xy)
                //{
                //    discard;
                //}

                float f_exp = apply_exposure(kf, exposure);
                float d_fexp_d_f = d_f_exp_d_f(kf, exposure);
                vec3 d_fexp_d_exp = d_f_exp_d_exp(kf, exposure);
                vec3 d_fexp_d_xy = d_fexp_d_f * d_f_d_xy;
                
                float v0 = d_fexp_d_xy.x * fx * out_width / f_ver.z;
                float v1 = d_fexp_d_xy.y * fy * out_height / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                vec3 d_f_i_d_tra = vec3(v0, v1, v2);
                vec3 d_f_i_d_rot = vec3(-f_ver.z * v1 + f_ver.y * v2,
                                         f_ver.z * v0 - f_ver.x * v2, 
                                        -f_ver.y * v0 + f_ver.x * v1);

                jtra_output = d_f_i_d_tra;
                jrot_output = d_f_i_d_rot;
                jexp_output = d_fexp_d_exp;
                image_output = f_exp;
            }
            )Shader";

        create_framebuffer(fbo_, rbo_);
        program_ = create_program(vertex_shader, nullptr, fragment_shader, common);

        view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");

        fx_loc_ = glGetUniformLocation(program_, "fx");
        fy_loc_ = glGetUniformLocation(program_, "fy");
        cx_loc_ = glGetUniformLocation(program_, "cx");
        cy_loc_ = glGetUniformLocation(program_, "cy");

        kf_image_loc_ = glGetUniformLocation(program_, "kf_image");
        kf_image_nodata_loc_ = glGetUniformLocation(program_, "kf_image_nodata");

        dfdxy_image_loc_ = glGetUniformLocation(program_, "dfdxy_image");
        dfdxy_image_nodata_loc_ = glGetUniformLocation(program_, "dfdxy_image_nodata");

        in_lvl_loc_ = glGetUniformLocation(program_, "in_lvl");
        out_lvl_loc_ = glGetUniformLocation(program_, "out_lvl");

        out_width_loc_ = glGetUniformLocation(program_, "out_width");
        out_height_loc_ = glGetUniformLocation(program_, "out_height");

        exposure_loc_ = glGetUniformLocation(program_, "exposure");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<ImageType> &kf_texture,
                const TextureGL<Vec3<float>> &dfdxy_texture,
                TextureGL<ImageType> &image_texture,
                TextureGL<Vec3<float>> &jtra_texture,
                TextureGL<Vec3<float>> &jrot_texture,
                TextureGL<Vec3<float>> &jexp_texture)
    {
        // save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, jtra_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, jrot_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, jexp_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, image_texture.id(), out_lvl);

        const GLenum bufs[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
        glDrawBuffers(4, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(jtra_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(jtra_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        Vec3<float> jtra_nodata = jtra_texture.nodata();
        Vec3<float> jrot_nodata = jrot_texture.nodata();
        Vec3<float> jexp_nodata = jexp_texture.nodata();
        ImageType image_nodata = image_texture.nodata();

        float jtra_clear[4] = {jtra_nodata(0), jtra_nodata(1), jtra_nodata(2), 1.f};
        float jrot_clear[4] = {jrot_nodata(0), jrot_nodata(1), jrot_nodata(2), 1.f};
        float jexp_clear[4] = {jexp_nodata(0), jexp_nodata(1), jexp_nodata(2), 1.f};
        float image_clear[4] = {image_nodata, 0.f, 0.f, 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, jtra_clear);
        //         glClearBufferfv(GL_COLOR, 1, jrot_clear);
        //         glClearBufferfv(GL_COLOR, 2, r_clear);
        // #else
        //  Clear GL_COLOR_ATTACHMENT0
        const GLenum bufs0[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs0);
        glClearColor(jtra_clear[0], jtra_clear[1], jtra_clear[2], jtra_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT1
        const GLenum bufs1[1] = {GL_COLOR_ATTACHMENT1};
        glDrawBuffers(1, bufs1);
        glClearColor(jrot_clear[0], jrot_clear[1], jrot_clear[2], jrot_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT2
        const GLenum bufs2[1] = {GL_COLOR_ATTACHMENT2};
        glDrawBuffers(1, bufs2);
        glClearColor(jexp_clear[0], jexp_clear[1], jexp_clear[2], jexp_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT3
        const GLenum bufs3[1] = {GL_COLOR_ATTACHMENT3};
        glDrawBuffers(1, bufs3);
        glClearColor(image_clear[0], image_clear[1], image_clear[2], image_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Restore glDrawBuffers for subsequent rendering.
        // This assumes the original setup was GL_COLOR_ATTACHMENT0 and GL_COLOR_ATTACHMENT1
        // as done in the clear_buffers function.
        const GLenum bufs_restore[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
        glDrawBuffers(4, bufs_restore);
        // #endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, kf_texture.id());
            glBindTextureUnit(1, dfdxy_texture.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, kf_texture.id());
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, dfdxy_texture.id());
        }

        glUseProgram(program_);

        const Mat4<float> view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        const Mat4<float> pose_matrix = pose.matrix();

        glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, view_matrix.data());
        glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, pose_matrix.data());

        glUniform1i(kf_image_loc_, 0);
        glUniform1f(kf_image_nodata_loc_, kf_texture.nodata());

        glUniform1i(dfdxy_image_loc_, 1);
        // glUniform1f(dfdxy_image_nodata_loc_, dfdxy_texture.nodata());

        glUniform1i(in_lvl_loc_, in_lvl);
        glUniform1i(out_lvl_loc_, out_lvl);

        glUniform1i(out_width_loc_, W);
        glUniform1i(out_height_loc_, H);

        glUniform1f(fx_loc_, cam.GetParams()(0));
        glUniform1f(fy_loc_, cam.GetParams()(1));
        glUniform1f(cx_loc_, cam.GetParams()(2));
        glUniform1f(cy_loc_, cam.GetParams()(3));

        glUniform2f(exposure_loc_, exposure(0), exposure(1));

        mesh.draw();

        // restore_state();
    }

private:
    GLuint fbo_;
    GLuint rbo_;
    GLuint program_;

    GLint view_matrix_loc_ = -1;
    GLint pose_matrix_loc_ = -1;

    GLint fx_loc_ = -1;
    GLint fy_loc_ = -1;
    GLint cx_loc_ = -1;
    GLint cy_loc_ = -1;

    GLint kf_image_loc_ = -1;
    GLint kf_image_nodata_loc_ = -1;

    GLint dfdxy_image_loc_ = -1;
    GLint dfdxy_image_nodata_loc_ = -1;

    GLint in_lvl_loc_ = -1;
    GLint out_lvl_loc_ = -1;

    GLint out_width_loc_ = -1;
    GLint out_height_loc_ = -1;

    GLint exposure_loc_ = -1;
};

class JDepthExpRendererGL
{
public:
    JDepthExpRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core

            layout (location = 0) in vec3 a_position;

            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec3 v_kf_ver;
            out vec3 v_f_ver;
            out vec3 v_kf_ray;
            flat out int v_vertexID;

            void main() {
                vec4 ver = pose_matrix * vec4(a_position, 1.0);
                vec3 rray = mat3(pose_matrix) * a_position/a_position.z;
                gl_Position = view_matrix * ver;
                v_f_ver = ver.xyz;
                v_kf_ray = rray.xyz;
                v_kf_ver = a_position;
                v_vertexID = gl_VertexID;
            }
            )Shader";

        const char *geometry_shader = R"Shader(
            #version 330 core

            layout(triangles) in;
            layout(triangle_strip, max_vertices = 3) out;

            in vec3 v_kf_ver[];
            in vec3 v_f_ver[];
            in vec3 v_kf_ray[];
            flat in int v_vertexID[];           // from VS (Option A)

            out vec3 kf_ver;
            out vec3 f_ver;
            flat out vec3 kf_ray_0;
            flat out vec3 kf_ray_1;
            flat out vec3 kf_ray_2;
            flat out ivec3 triIDs;         // to FS: the 3 vertex IDs of this triangle
            smooth out vec3  bc;           // perspective-correct barycentrics to FS
            // noperspective out vec3 bc;  // uncomment for screen-space-linear barycentrics

            // Option B: fetch element indices from a texture buffer that mirrors your EBO
            // uniform usamplerBuffer uIndexBuf;  // each texel = one uint index

            void main() {
                // Build the per-triangle ID triplet
                // Option A: use IDs passed from VS
                ivec3 ids = ivec3(v_vertexID[0], v_vertexID[1], v_vertexID[2]);

                // Option B: if using a TBO that mirrors your index buffer:
                // uint base = 3u * uint(gl_PrimitiveIDIn);
                // uvec3 ids = uvec3(
                //     texelFetch(uIndexBuf, int(base+0)).x,
                //     texelFetch(uIndexBuf, int(base+1)).x,
                //     texelFetch(uIndexBuf, int(base+2)).x
                // );

                kf_ray_0 = v_kf_ray[0];
                kf_ray_1 = v_kf_ray[1];
                kf_ray_2 = v_kf_ray[2];
                triIDs = ids;

                for (int i = 0; i < 3; ++i) {
                    kf_ver = v_kf_ver[i];
                    f_ver = v_f_ver[i];
                    bc     = vec3(i == 0, i == 1, i == 2);
                    gl_Position = gl_in[i].gl_Position;
                    EmitVertex();
                }
                EndPrimitive();
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core

            layout(location = 0) out vec3 jmap_output;
            layout(location = 1) out vec3 jexp_output;
            layout(location = 2) out vec3 pids_output;
            layout(location = 3) out float image_output;

            in vec3 kf_ver;
            in vec3 f_ver;
            flat in vec3 kf_ray_0;
            flat in vec3 kf_ray_1;
            flat in vec3 kf_ray_2;

            smooth in vec3  bc;          // or noperspective if chosen above
            flat   in ivec3 triIDs;

            uniform sampler2D kf_image;
            uniform float kf_image_nodata;

            uniform sampler2D dfdxy_image;
            uniform vec3 dfdxy_image_nodata;

            uniform int in_lvl;
            uniform int out_lvl;

            uniform int out_width;
            uniform int out_height;

            uniform float fx;
            uniform float fy;
            uniform float cx;
            uniform float cy;

            uniform vec2 exposure;

            void main()
            {
                // ivec2 tex_size = textureSize(f_image, f_image_lvl);

                vec2 texcoord = pointToPix(kf_ver, fx, fy, cx, cy);
                if(texcoord.x < 0.0 || texcoord.x > 1.0 || texcoord.y < 0.0 || texcoord.y > 1.0)
                {
                    discard;
                }

                float kf = textureLod(kf_image, texcoord, float(in_lvl)).r;
                // vec3 d_f_d_xy = texelFetch(dfdxy_image, ivec2(gl_FragCoord.x, gl_FragCoord.y), out_lvl).xy;
                vec3 d_f_d_xy = textureLod(dfdxy_image, texcoord, float(in_lvl)).xyz;

                //if (kf == kf_image_nodata || f == f_image_nodata || dfdxy.xy == dfdxy_image_nodata.xy)
                //{
                //    discard;
                //}

                float f_exp = apply_exposure(kf, exposure);
                float d_fexp_d_f = d_f_exp_d_f(kf, exposure);
                vec3 d_fexp_d_exp = d_f_exp_d_exp(kf, exposure);
                vec3 d_fexp_d_xy = d_fexp_d_f * d_f_d_xy;

                float v0 = d_fexp_d_xy.x * fx * out_width / f_ver.z;
                float v1 = d_fexp_d_xy.y * fy * out_height / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                vec3 d_f_i_d_f_ver = vec3(v0, v1, v2);
                //vec3f d_f_i_d_rot = vec3(-f_ver.z * v1 + f_ver.y * v2, f_ver.z * v0 - f_ver.x * v2, -f_ver.y * v0 + f_ver.x * v1);

                //jtra_output = d_f_i_d_tra;
                //jrot_output = d_f_i_d_rot;

                float d_f_i_d_kf_depth_0 = dot(d_f_i_d_f_ver, kf_ray_0);
                float d_f_i_d_kf_depth_1 = dot(d_f_i_d_f_ver, kf_ray_1);
                float d_f_i_d_kf_depth_2 = dot(d_f_i_d_f_ver, kf_ray_2);

                vec3 d_depth_d_vert_depth = bc;

                vec3 jac;
                jac.x = d_f_i_d_kf_depth_0 * d_depth_d_vert_depth.x;
                jac.y = d_f_i_d_kf_depth_1 * d_depth_d_vert_depth.y;
                jac.z = d_f_i_d_kf_depth_2 * d_depth_d_vert_depth.z;

                jmap_output = jac;
                jexp_output = d_fexp_d_exp;
                pids_output = triIDs;
                image_output = f_exp;
            }
            )Shader";

        create_framebuffer(fbo_, rbo_);
        program_ = create_program(vertex_shader, geometry_shader, fragment_shader, common);

        view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");

        fx_loc_ = glGetUniformLocation(program_, "fx");
        fy_loc_ = glGetUniformLocation(program_, "fy");
        cx_loc_ = glGetUniformLocation(program_, "cx");
        cy_loc_ = glGetUniformLocation(program_, "cy");

        kf_image_loc_ = glGetUniformLocation(program_, "kf_image");
        kf_image_nodata_loc_ = glGetUniformLocation(program_, "kf_image_nodata");

        dfdxy_image_loc_ = glGetUniformLocation(program_, "dfdxy_image");
        dfdxy_image_nodata_loc_ = glGetUniformLocation(program_, "dfdxy_image_nodata");

        in_lvl_loc_ = glGetUniformLocation(program_, "in_lvl");
        out_lvl_loc_ = glGetUniformLocation(program_, "out_lvl");

        out_width_loc_ = glGetUniformLocation(program_, "out_width");
        out_height_loc_ = glGetUniformLocation(program_, "out_height");

        exposure_loc_ = glGetUniformLocation(program_, "exposure");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<ImageType> &kf_texture,
                const TextureGL<Vec3<float>> &dfdxy_texture,
                TextureGL<ImageType> &image_texture,
                TextureGL<Vec3<float>> &jdepth_texture,
                TextureGL<Vec3<float>> &jexp_texture,
                TextureGL<Vec3<PidType>> &pids_texture)
    {
        // save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, jdepth_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, jexp_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, pids_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, image_texture.id(), out_lvl);

        const GLenum bufs[4] = {GL_COLOR_ATTACHMENT0,
                                GL_COLOR_ATTACHMENT1,
                                GL_COLOR_ATTACHMENT2,
                                GL_COLOR_ATTACHMENT3};
        glDrawBuffers(4, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(jdepth_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(jdepth_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        Vec3<float> jdepth_nodata = jdepth_texture.nodata();
        Vec3<float> jexp_nodata = jexp_texture.nodata();
        Vec3<float> pids_nodata = pids_texture.nodata();
        ImageType image_nodata = image_texture.nodata();

        float jdepth_clear[4] = {jdepth_nodata(0), jdepth_nodata(1), jdepth_nodata(2), 1.f};
        float jexp_clear[4] = {jexp_nodata(0), jexp_nodata(1), jexp_nodata(2), 1.f};
        float pids_clear[4] = {pids_nodata(0), pids_nodata(1), pids_nodata(2), 1.f};
        float image_clear[4] = {image_nodata, 0.f, 0.f, 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, jmap_clear);
        //         glClearBufferfv(GL_COLOR, 1, pids_clear);
        //         glClearBufferfv(GL_COLOR, 2, r_clear);
        // #else
        //  Clear GL_COLOR_ATTACHMENT0
        const GLenum bufs0[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs0);
        glClearColor(jdepth_clear[0], jdepth_clear[1], jdepth_clear[2], jdepth_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT2
        const GLenum bufs1[1] = {GL_COLOR_ATTACHMENT1};
        glDrawBuffers(1, bufs1);
        glClearColor(jexp_clear[0], jexp_clear[1], jexp_clear[2], jexp_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT1
        const GLenum bufs2[1] = {GL_COLOR_ATTACHMENT2};
        glDrawBuffers(1, bufs2);
        glClearColor(pids_clear[0], pids_clear[1], pids_clear[2], pids_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT2
        const GLenum bufs3[1] = {GL_COLOR_ATTACHMENT3};
        glDrawBuffers(1, bufs3);
        glClearColor(image_clear[0], image_clear[1], image_clear[2], image_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Restore glDrawBuffers for subsequent rendering.
        // This assumes the original setup was GL_COLOR_ATTACHMENT0 and GL_COLOR_ATTACHMENT1
        // as done in the clear_buffers function.
        glDrawBuffers(4, bufs);
        // #endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, kf_texture.id());
            glBindTextureUnit(1, dfdxy_texture.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, kf_texture.id());
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, dfdxy_texture.id());
        }

        glUseProgram(program_);

        const Mat4<float> view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        const Mat4<float> pose_matrix = pose.matrix();

        glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, view_matrix.data());
        glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, pose_matrix.data());

        glUniform1i(kf_image_loc_, 0);
        glUniform1f(kf_image_nodata_loc_, kf_texture.nodata());

        glUniform1i(dfdxy_image_loc_, 1);
        // glUniform3f(dfdxy_image_nodata_loc_, dfdxy_texture.nodata());

        glUniform1i(in_lvl_loc_, in_lvl);
        glUniform1i(out_lvl_loc_, out_lvl);

        glUniform1i(out_width_loc_, W);
        glUniform1i(out_height_loc_, H);

        glUniform1f(fx_loc_, cam.GetParams()(0));
        glUniform1f(fy_loc_, cam.GetParams()(1));
        glUniform1f(cx_loc_, cam.GetParams()(2));
        glUniform1f(cy_loc_, cam.GetParams()(3));

        glUniform2f(exposure_loc_, exposure(0), exposure(1));

        mesh.draw();

        // restore_state();
    }

private:
    GLuint fbo_;
    GLuint rbo_;
    GLuint program_;

    GLint view_matrix_loc_ = -1;
    GLint pose_matrix_loc_ = -1;

    GLint fx_loc_ = -1;
    GLint fy_loc_ = -1;
    GLint cx_loc_ = -1;
    GLint cy_loc_ = -1;

    GLint kf_image_loc_ = -1;
    GLint kf_image_nodata_loc_ = -1;

    GLint dfdxy_image_loc_ = -1;
    GLint dfdxy_image_nodata_loc_ = -1;

    GLint in_lvl_loc_ = -1;
    GLint out_lvl_loc_ = -1;

    GLint out_width_loc_ = -1;
    GLint out_height_loc_ = -1;

    GLint exposure_loc_ = -1;
};

class JRayDepthExpRendererGL
{
public:
    JRayDepthExpRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core

            layout (location = 0) in vec3 a_position;

            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec3  v_kf_ver;      // keyframe vertex position (for interpolation / texcoord)
            out vec3  v_f_ver;       // current-frame vertex position
            out vec3  v_ray_kf;      // keyframe ray (k = d * ray)
            out float v_depth;       // depth d
            flat out int v_vertexID;

            void main() {
                // depth/ray parameterization at the linearization point
                float d = a_position.z;
                float invd = 1.0 / max(d, 1e-8);
                vec3 ray_kf = a_position * invd;  // ray_kf.z ~ 1 if you use that convention

                vec4 ver_f = pose_matrix * vec4(a_position, 1.0);

                gl_Position = view_matrix * ver_f;

                v_kf_ver   = a_position;
                v_f_ver    = ver_f.xyz;
                v_ray_kf   = ray_kf;
                v_depth    = d;
                v_vertexID = gl_VertexID;
            }
        )Shader";

        const char *geometry_shader = R"Shader(
            #version 330 core

            layout(triangles) in;
            layout(triangle_strip, max_vertices = 3) out;

            in vec3  v_kf_ver[];
            in vec3  v_f_ver[];
            in vec3  v_ray_kf[];
            in float v_depth[];
            flat in int v_vertexID[];

            out vec3 kf_ver;
            out vec3 f_ver;

            flat out vec3  ray_kf_0;
            flat out vec3  ray_kf_1;
            flat out vec3  ray_kf_2;
            flat out vec3  depth_012;   // (d0,d1,d2)

            flat out ivec3 triIDs;
            smooth out vec3 bc;         // perspective-correct barycentrics

            void main() {
                triIDs = ivec3(v_vertexID[0], v_vertexID[1], v_vertexID[2]);

                ray_kf_0  = v_ray_kf[0];
                ray_kf_1  = v_ray_kf[1];
                ray_kf_2  = v_ray_kf[2];
                depth_012 = vec3(v_depth[0], v_depth[1], v_depth[2]);

                for (int i = 0; i < 3; ++i) {
                    kf_ver = v_kf_ver[i];
                    f_ver  = v_f_ver[i];
                    bc     = vec3(i == 0, i == 1, i == 2);
                    gl_Position = gl_in[i].gl_Position;
                    EmitVertex();
                }
                EndPrimitive();
            }
        )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core

            // Geometry Jacobians (per-triangle, per-vertex, packed by component)
            layout(location = 0) out vec3 j_depth_012; // dI/dd0, dI/dd1, dI/dd2
            layout(location = 1) out vec3 j_rayx_012;  // dI/dr0.x, dI/dr1.x, dI/dr2.x
            layout(location = 2) out vec3 j_rayy_012;  // dI/dr0.y, dI/dr1.y, dI/dr2.y
            layout(location = 3) out vec3 j_rayz_012;  // dI/dr0.z, dI/dr1.z, dI/dr2.z

            // Keep the other outputs (shifted)
            layout(location = 4) out vec3  jexp_output;
            layout(location = 5) out vec3  pids_output;
            layout(location = 6) out float image_output;

            in vec3 kf_ver;
            in vec3 f_ver;

            flat in vec3 ray_kf_0;
            flat in vec3 ray_kf_1;
            flat in vec3 ray_kf_2;
            flat in vec3 depth_012;

            smooth in vec3  bc;
            flat   in ivec3 triIDs;

            uniform mat4 pose_matrix;   // needed here to get R^T

            uniform sampler2D kf_image;
            uniform float kf_image_nodata;

            uniform sampler2D dfdxy_image;
            uniform vec3 dfdxy_image_nodata;

            uniform int in_lvl;
            uniform int out_lvl;

            uniform int out_width;
            uniform int out_height;

            uniform float fx;
            uniform float fy;
            uniform float cx;
            uniform float cy;

            uniform vec2 exposure;

            void main()
            {
                vec2 texcoord = pointToPix(kf_ver, fx, fy, cx, cy);
                if(texcoord.x < 0.0 || texcoord.x > 1.0 || texcoord.y < 0.0 || texcoord.y > 1.0)
                    discard;

                float kf = textureLod(kf_image, texcoord, float(in_lvl)).r;
                vec3 d_f_d_xy = textureLod(dfdxy_image, texcoord, float(in_lvl)).xyz;

                float f_exp = apply_exposure(kf, exposure);
                float d_fexp_d_f = d_f_exp_d_f(kf, exposure);
                vec3  d_fexp_d_exp = d_f_exp_d_exp(kf, exposure);
                vec3  d_fexp_d_xy  = d_fexp_d_f * d_f_d_xy;

                // dI/df_ver (current-frame 3D point)
                float v0 = d_fexp_d_xy.x * fx * out_width  / f_ver.z;
                float v1 = d_fexp_d_xy.y * fy * out_height / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;
                vec3 dI_d_fver = vec3(v0, v1, v2);

                // dI/dk (keyframe 3D point): R^T * dI/df
                mat3 R = mat3(pose_matrix);
                vec3 dI_d_k = transpose(R) * dI_d_fver;

                // Depth jacobians: bc_i * (dI/dk · ray_i)
                float j_d0 = bc.x * dot(dI_d_k, ray_kf_0);
                float j_d1 = bc.y * dot(dI_d_k, ray_kf_1);
                float j_d2 = bc.z * dot(dI_d_k, ray_kf_2);
                j_depth_012 = vec3(j_d0, j_d1, j_d2);

                // Ray jacobians: bc_i * d_i * dI/dk  (component-wise)
                vec3 j_r0 = bc.x * depth_012.x * dI_d_k;
                vec3 j_r1 = bc.y * depth_012.y * dI_d_k;
                vec3 j_r2 = bc.z * depth_012.z * dI_d_k;

                j_rayx_012 = vec3(j_r0.x, j_r1.x, j_r2.x);
                j_rayy_012 = vec3(j_r0.y, j_r1.y, j_r2.y);
                j_rayz_012 = vec3(j_r0.z, j_r1.z, j_r2.z);

                jexp_output  = d_fexp_d_exp;
                pids_output  = vec3(triIDs);
                image_output = f_exp;
            }
        )Shader";

        create_framebuffer(fbo_, rbo_);
        program_ = create_program(vertex_shader, geometry_shader, fragment_shader, common);

        view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");

        fx_loc_ = glGetUniformLocation(program_, "fx");
        fy_loc_ = glGetUniformLocation(program_, "fy");
        cx_loc_ = glGetUniformLocation(program_, "cx");
        cy_loc_ = glGetUniformLocation(program_, "cy");

        kf_image_loc_ = glGetUniformLocation(program_, "kf_image");
        kf_image_nodata_loc_ = glGetUniformLocation(program_, "kf_image_nodata");

        dfdxy_image_loc_ = glGetUniformLocation(program_, "dfdxy_image");
        dfdxy_image_nodata_loc_ = glGetUniformLocation(program_, "dfdxy_image_nodata");

        in_lvl_loc_ = glGetUniformLocation(program_, "in_lvl");
        out_lvl_loc_ = glGetUniformLocation(program_, "out_lvl");

        out_width_loc_ = glGetUniformLocation(program_, "out_width");
        out_height_loc_ = glGetUniformLocation(program_, "out_height");

        exposure_loc_ = glGetUniformLocation(program_, "exposure");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<ImageType> &kf_texture,
                const TextureGL<Vec3<float>> &dfdxy_texture,
                TextureGL<ImageType> &image_texture,
                TextureGL<Vec3<float>> &jdepth_texture,
                TextureGL<Vec3<float>> &jray0_texture,
                TextureGL<Vec3<float>> &jray1_texture,
                TextureGL<Vec3<float>> &jray2_texture,
                TextureGL<Vec3<float>> &jexp_texture,
                TextureGL<Vec3<PidType>> &pids_texture)
    {
        // save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, jdepth_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, jexp_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, pids_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, image_texture.id(), out_lvl);

        const GLenum bufs[4] = {GL_COLOR_ATTACHMENT0,
                                GL_COLOR_ATTACHMENT1,
                                GL_COLOR_ATTACHMENT2,
                                GL_COLOR_ATTACHMENT3};
        glDrawBuffers(4, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(jdepth_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(jdepth_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        Vec3<float> jdepth_nodata = jdepth_texture.nodata();
        Vec3<float> jexp_nodata = jexp_texture.nodata();
        Vec3<float> pids_nodata = pids_texture.nodata();
        ImageType image_nodata = image_texture.nodata();

        float jdepth_clear[4] = {jdepth_nodata(0), jdepth_nodata(1), jdepth_nodata(2), 1.f};
        float jexp_clear[4] = {jexp_nodata(0), jexp_nodata(1), jexp_nodata(2), 1.f};
        float pids_clear[4] = {pids_nodata(0), pids_nodata(1), pids_nodata(2), 1.f};
        float image_clear[4] = {image_nodata, 0.f, 0.f, 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, jmap_clear);
        //         glClearBufferfv(GL_COLOR, 1, pids_clear);
        //         glClearBufferfv(GL_COLOR, 2, r_clear);
        // #else
        //  Clear GL_COLOR_ATTACHMENT0
        const GLenum bufs0[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs0);
        glClearColor(jdepth_clear[0], jdepth_clear[1], jdepth_clear[2], jdepth_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT2
        const GLenum bufs1[1] = {GL_COLOR_ATTACHMENT1};
        glDrawBuffers(1, bufs1);
        glClearColor(jexp_clear[0], jexp_clear[1], jexp_clear[2], jexp_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT1
        const GLenum bufs2[1] = {GL_COLOR_ATTACHMENT2};
        glDrawBuffers(1, bufs2);
        glClearColor(pids_clear[0], pids_clear[1], pids_clear[2], pids_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT2
        const GLenum bufs3[1] = {GL_COLOR_ATTACHMENT3};
        glDrawBuffers(1, bufs3);
        glClearColor(image_clear[0], image_clear[1], image_clear[2], image_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Restore glDrawBuffers for subsequent rendering.
        // This assumes the original setup was GL_COLOR_ATTACHMENT0 and GL_COLOR_ATTACHMENT1
        // as done in the clear_buffers function.
        glDrawBuffers(4, bufs);
        // #endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, kf_texture.id());
            glBindTextureUnit(1, dfdxy_texture.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, kf_texture.id());
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, dfdxy_texture.id());
        }

        glUseProgram(program_);

        const Mat4<float> view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        const Mat4<float> pose_matrix = pose.matrix();

        glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, view_matrix.data());
        glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, pose_matrix.data());

        glUniform1i(kf_image_loc_, 0);
        glUniform1f(kf_image_nodata_loc_, kf_texture.nodata());

        glUniform1i(dfdxy_image_loc_, 1);
        // glUniform3f(dfdxy_image_nodata_loc_, dfdxy_texture.nodata());

        glUniform1i(in_lvl_loc_, in_lvl);
        glUniform1i(out_lvl_loc_, out_lvl);

        glUniform1i(out_width_loc_, W);
        glUniform1i(out_height_loc_, H);

        glUniform1f(fx_loc_, cam.GetParams()(0));
        glUniform1f(fy_loc_, cam.GetParams()(1));
        glUniform1f(cx_loc_, cam.GetParams()(2));
        glUniform1f(cy_loc_, cam.GetParams()(3));

        glUniform2f(exposure_loc_, exposure(0), exposure(1));

        mesh.draw();

        // restore_state();
    }

private:
    GLuint fbo_;
    GLuint rbo_;
    GLuint program_;

    GLint view_matrix_loc_ = -1;
    GLint pose_matrix_loc_ = -1;

    GLint fx_loc_ = -1;
    GLint fy_loc_ = -1;
    GLint cx_loc_ = -1;
    GLint cy_loc_ = -1;

    GLint kf_image_loc_ = -1;
    GLint kf_image_nodata_loc_ = -1;

    GLint dfdxy_image_loc_ = -1;
    GLint dfdxy_image_nodata_loc_ = -1;

    GLint in_lvl_loc_ = -1;
    GLint out_lvl_loc_ = -1;

    GLint out_width_loc_ = -1;
    GLint out_height_loc_ = -1;

    GLint exposure_loc_ = -1;
};

class JVertexExpRendererGL
{
public:
    JVertexExpRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core

            layout (location = 0) in vec3 a_position;

            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec3 v_kf_ver;
            out vec3 v_f_ver;
            flat out int v_vertexID;

            void main() {
                vec4 ver = pose_matrix * vec4(a_position, 1.0);
                gl_Position = view_matrix * ver;

                v_f_ver = ver.xyz;       // point in current frame
                v_kf_ver = a_position;   // point in keyframe frame
                v_vertexID = gl_VertexID;
            }
        )Shader";

        const char *geometry_shader = R"Shader(
            #version 330 core

            layout(triangles) in;
            layout(triangle_strip, max_vertices = 3) out;

            in vec3 v_kf_ver[];
            in vec3 v_f_ver[];
            flat in int v_vertexID[];

            out vec3 kf_ver;
            out vec3 f_ver;

            flat out ivec3 triIDs;
            smooth out vec3  bc;   // perspective-correct barycentrics

            void main() {
                triIDs = ivec3(v_vertexID[0], v_vertexID[1], v_vertexID[2]);

                for (int i = 0; i < 3; ++i) {
                    kf_ver = v_kf_ver[i];
                    f_ver  = v_f_ver[i];
                    bc     = vec3(i == 0, i == 1, i == 2);
                    gl_Position = gl_in[i].gl_Position;
                    EmitVertex();
                }
                EndPrimitive();
            }
        )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core

            // Per-vertex XYZ Jacobians (keyframe vertex coordinates)
            layout(location = 0) out vec3 j_v0_xyz;
            layout(location = 1) out vec3 j_v1_xyz;
            layout(location = 2) out vec3 j_v2_xyz;

            // Keep the other outputs (shifted to higher locations)
            layout(location = 3) out vec3 jexp_output;
            layout(location = 4) out vec3 pids_output;
            layout(location = 5) out float image_output;

            in vec3 kf_ver;
            in vec3 f_ver;

            smooth in vec3  bc;
            flat   in ivec3 triIDs;

            uniform mat4 pose_matrix;   // <-- add pose_matrix to FS too

            uniform sampler2D kf_image;
            uniform float kf_image_nodata;

            uniform sampler2D dfdxy_image;
            uniform vec3 dfdxy_image_nodata;

            uniform int in_lvl;
            uniform int out_lvl;

            uniform int out_width;
            uniform int out_height;

            uniform float fx;
            uniform float fy;
            uniform float cx;
            uniform float cy;

            uniform vec2 exposure;

            void main()
            {
                vec2 texcoord = pointToPix(kf_ver, fx, fy, cx, cy);
                if(texcoord.x < 0.0 || texcoord.x > 1.0 || texcoord.y < 0.0 || texcoord.y > 1.0)
                    discard;

                float kf = textureLod(kf_image, texcoord, float(in_lvl)).r;
                vec3 d_f_d_xy = textureLod(dfdxy_image, texcoord, float(in_lvl)).xyz;

                float f_exp = apply_exposure(kf, exposure);
                float d_fexp_d_f = d_f_exp_d_f(kf, exposure);
                vec3  d_fexp_d_exp = d_f_exp_d_exp(kf, exposure);
                vec3  d_fexp_d_xy  = d_fexp_d_f * d_f_d_xy;

                // dI/df_ver (current-frame 3D point)
                float v0 = d_fexp_d_xy.x * fx * out_width  / f_ver.z;
                float v1 = d_fexp_d_xy.y * fy * out_height / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                vec3 dI_d_fver = vec3(v0, v1, v2);

                // Convert to keyframe coordinates: dI/dk = R^T * dI/df
                mat3 R = mat3(pose_matrix);
                vec3 dI_d_kf = transpose(R) * dI_d_fver;

                // Each vertex affects kf_ver by its barycentric weight
                j_v0_xyz = bc.x * dI_d_kf;   // ∂I/∂(x0,y0,z0)
                j_v1_xyz = bc.y * dI_d_kf;   // ∂I/∂(x1,y1,z1)
                j_v2_xyz = bc.z * dI_d_kf;   // ∂I/∂(x2,y2,z2)

                jexp_output = d_fexp_d_exp;
                pids_output = vec3(triIDs);
                image_output = f_exp;
            }
        )Shader";

        create_framebuffer(fbo_, rbo_);
        program_ = create_program(vertex_shader, geometry_shader, fragment_shader, common);

        view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");

        fx_loc_ = glGetUniformLocation(program_, "fx");
        fy_loc_ = glGetUniformLocation(program_, "fy");
        cx_loc_ = glGetUniformLocation(program_, "cx");
        cy_loc_ = glGetUniformLocation(program_, "cy");

        kf_image_loc_ = glGetUniformLocation(program_, "kf_image");
        kf_image_nodata_loc_ = glGetUniformLocation(program_, "kf_image_nodata");

        dfdxy_image_loc_ = glGetUniformLocation(program_, "dfdxy_image");
        dfdxy_image_nodata_loc_ = glGetUniformLocation(program_, "dfdxy_image_nodata");

        in_lvl_loc_ = glGetUniformLocation(program_, "in_lvl");
        out_lvl_loc_ = glGetUniformLocation(program_, "out_lvl");

        out_width_loc_ = glGetUniformLocation(program_, "out_width");
        out_height_loc_ = glGetUniformLocation(program_, "out_height");

        exposure_loc_ = glGetUniformLocation(program_, "exposure");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<ImageType> &kf_texture,
                const TextureGL<Vec3<float>> &dfdxy_texture,
                TextureGL<ImageType> &image_texture,
                TextureGL<Vec3<float>> &jv0_texture,
                TextureGL<Vec3<float>> &jv1_texture,
                TextureGL<Vec3<float>> &jv2_texture,
                TextureGL<Vec3<float>> &jexp_texture,
                TextureGL<Vec3<PidType>> &pids_texture)
    {
        // save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, jv0_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, jv1_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, jv2_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, jexp_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, pids_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, image_texture.id(), out_lvl);

        const GLenum bufs[6] = {GL_COLOR_ATTACHMENT0,
                                GL_COLOR_ATTACHMENT1,
                                GL_COLOR_ATTACHMENT2,
                                GL_COLOR_ATTACHMENT3,
                                GL_COLOR_ATTACHMENT4,
                                GL_COLOR_ATTACHMENT5};
        glDrawBuffers(6, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(image_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(image_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        Vec3<float> jv0_nodata = jv0_texture.nodata();
        Vec3<float> jv1_nodata = jv1_texture.nodata();
        Vec3<float> jv2_nodata = jv2_texture.nodata();
        Vec3<float> jexp_nodata = jexp_texture.nodata();
        Vec3<float> pids_nodata = pids_texture.nodata();
        ImageType image_nodata = image_texture.nodata();

        float jv0_clear[4] = {jv0_nodata(0), jv0_nodata(1), jv0_nodata(2), 1.f};
        float jv1_clear[4] = {jv1_nodata(0), jv1_nodata(1), jv1_nodata(2), 1.f};
        float jv2_clear[4] = {jv2_nodata(0), jv2_nodata(1), jv2_nodata(2), 1.f};
        float jexp_clear[4] = {jexp_nodata(0), jexp_nodata(1), jexp_nodata(2), 1.f};
        float pids_clear[4] = {pids_nodata(0), pids_nodata(1), pids_nodata(2), 1.f};
        float image_clear[4] = {image_nodata, 0.f, 0.f, 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, jmap_clear);
        //         glClearBufferfv(GL_COLOR, 1, pids_clear);
        //         glClearBufferfv(GL_COLOR, 2, r_clear);
        // #else
        //  Clear GL_COLOR_ATTACHMENT0
        GLenum buf_to_clear[1];

        buf_to_clear[0] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, buf_to_clear);
        glClearColor(jv0_clear[0], jv0_clear[1], jv0_clear[2], jv0_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        buf_to_clear[0] = {GL_COLOR_ATTACHMENT1};
        glDrawBuffers(1, buf_to_clear);
        glClearColor(jv1_clear[0], jv1_clear[1], jv1_clear[2], jv1_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        buf_to_clear[0] = {GL_COLOR_ATTACHMENT2};
        glDrawBuffers(1, buf_to_clear);
        glClearColor(jv2_clear[0], jv2_clear[1], jv2_clear[2], jv2_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        buf_to_clear[0] = {GL_COLOR_ATTACHMENT2};

        glDrawBuffers(1, buf_to_clear);
        glClearColor(jexp_clear[0], jexp_clear[1], jexp_clear[2], jexp_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        buf_to_clear[0] = {GL_COLOR_ATTACHMENT4};
        glDrawBuffers(1, buf_to_clear);
        glClearColor(pids_clear[0], pids_clear[1], pids_clear[2], pids_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT2
        buf_to_clear[0] = {GL_COLOR_ATTACHMENT5};
        glDrawBuffers(1, buf_to_clear);
        glClearColor(image_clear[0], image_clear[1], image_clear[2], image_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Restore glDrawBuffers for subsequent rendering.
        // This assumes the original setup was GL_COLOR_ATTACHMENT0 and GL_COLOR_ATTACHMENT1
        // as done in the clear_buffers function.
        glDrawBuffers(6, bufs);
        // #endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, kf_texture.id());
            glBindTextureUnit(1, dfdxy_texture.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, kf_texture.id());
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, dfdxy_texture.id());
        }

        glUseProgram(program_);

        const Mat4<float> view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        const Mat4<float> pose_matrix = pose.matrix();

        glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, view_matrix.data());
        glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, pose_matrix.data());

        glUniform1i(kf_image_loc_, 0);
        glUniform1f(kf_image_nodata_loc_, kf_texture.nodata());

        glUniform1i(dfdxy_image_loc_, 1);
        // glUniform3f(dfdxy_image_nodata_loc_, dfdxy_texture.nodata());

        glUniform1i(in_lvl_loc_, in_lvl);
        glUniform1i(out_lvl_loc_, out_lvl);

        glUniform1i(out_width_loc_, W);
        glUniform1i(out_height_loc_, H);

        glUniform1f(fx_loc_, cam.GetParams()(0));
        glUniform1f(fy_loc_, cam.GetParams()(1));
        glUniform1f(cx_loc_, cam.GetParams()(2));
        glUniform1f(cy_loc_, cam.GetParams()(3));

        glUniform2f(exposure_loc_, exposure(0), exposure(1));

        mesh.draw();

        // restore_state();
    }

private:
    GLuint fbo_;
    GLuint rbo_;
    GLuint program_;

    GLint view_matrix_loc_ = -1;
    GLint pose_matrix_loc_ = -1;

    GLint fx_loc_ = -1;
    GLint fy_loc_ = -1;
    GLint cx_loc_ = -1;
    GLint cy_loc_ = -1;

    GLint kf_image_loc_ = -1;
    GLint kf_image_nodata_loc_ = -1;

    GLint dfdxy_image_loc_ = -1;
    GLint dfdxy_image_nodata_loc_ = -1;

    GLint in_lvl_loc_ = -1;
    GLint out_lvl_loc_ = -1;

    GLint out_width_loc_ = -1;
    GLint out_height_loc_ = -1;

    GLint exposure_loc_ = -1;
};

class JPoseExpDepthRendererGL
{
public:
    JPoseExpDepthRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core

            layout (location = 0) in vec3 a_position;

            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec3 v_kf_ver;
            out vec3 v_f_ver;
            out vec3 v_kf_ray;
            flat out int v_vertexID;

            void main() {
                vec4 ver = pose_matrix * vec4(a_position, 1.0);
                vec3 rray = mat3(pose_matrix) * a_position/a_position.z;
                gl_Position = view_matrix * ver;
                v_kf_ver = a_position;
                v_f_ver = ver.xyz;
                v_kf_ray = rray.xyz;
                v_vertexID = gl_VertexID;
            }
            )Shader";

        const char *geometry_shader = R"Shader(
            #version 330 core

            layout(triangles) in;
            layout(triangle_strip, max_vertices = 3) out;

            in vec3 v_kf_ver[];
            in vec3 v_f_ver[];
            in vec3 v_kf_ray[];
            flat in int v_vertexID[];           // from VS (Option A)

            out vec3 kf_ver;
            out vec3 f_ver;
            flat out vec3 kf_ray_0;
            flat out vec3 kf_ray_1;
            flat out vec3 kf_ray_2;
            flat out ivec3 triIDs;         // to FS: the 3 vertex IDs of this triangle
            smooth out vec3  bc;           // perspective-correct barycentrics to FS
            // noperspective out vec3 bc;  // uncomment for screen-space-linear barycentrics

            // Option B: fetch element indices from a texture buffer that mirrors your EBO
            // uniform usamplerBuffer uIndexBuf;  // each texel = one uint index

            void main() {
                // Build the per-triangle ID triplet
                // Option A: use IDs passed from VS
                ivec3 ids = ivec3(v_vertexID[0], v_vertexID[1], v_vertexID[2]);

                // Option B: if using a TBO that mirrors your index buffer:
                // uint base = 3u * uint(gl_PrimitiveIDIn);
                // uvec3 ids = uvec3(
                //     texelFetch(uIndexBuf, int(base+0)).x,
                //     texelFetch(uIndexBuf, int(base+1)).x,
                //     texelFetch(uIndexBuf, int(base+2)).x
                // );

                kf_ray_0 = v_kf_ray[0];
                kf_ray_1 = v_kf_ray[1];
                kf_ray_2 = v_kf_ray[2];

                for (int i = 0; i < 3; ++i) {
                    kf_ver = v_kf_ver[i];
                    f_ver = v_f_ver[i];
                    triIDs = ids;
                    bc     = vec3(i == 0, i == 1, i == 2);
                    gl_Position = gl_in[i].gl_Position;
                    EmitVertex();
                }
                EndPrimitive();
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core

            layout(location = 0) out vec3 jtra_output;
            layout(location = 1) out vec3 jrot_output;
            layout(location = 2) out vec3 jexp_output;
            layout(location = 3) out vec3 jmap_output;
            layout(location = 4) out vec3 pids_output;
            layout(location = 5) out float image_output;

            in vec3 kf_ver;
            in vec3 f_ver;
            flat in vec3 kf_ray_0;
            flat in vec3 kf_ray_1;
            flat in vec3 kf_ray_2;

            smooth in vec3  bc;          // or noperspective if chosen above
            flat   in ivec3 triIDs;

            uniform sampler2D kf_image;
            uniform float kf_image_nodata;

            uniform sampler2D f_image;
            uniform float f_image_nodata;

            uniform sampler2D dfdxy_image;
            uniform vec3 dfdxy_image_nodata;

            uniform int in_lvl;
            uniform int out_lvl;

            uniform int out_width;
            uniform int out_height;

            uniform float fx;
            uniform float fy;
            uniform float cx;
            uniform float cy;

            uniform vec2 exposure;

            void main()
            {

                vec2 texcoord = pointToPix(kf_ver, fx, fy, cx, cy);
                if(texcoord.x < 0.0 || texcoord.x > 1.0 || texcoord.y < 0.0 || texcoord.y > 1.0)
                {
                    discard;
                }

                float kf = textureLod(kf_image, texcoord, float(in_lvl)).r;
                // vec2 d_f_d_xy = texelFetch(dfdxy_image, ivec2(gl_FragCoord.x, gl_FragCoord.y), out_lvl).xy;
                vec3 d_f_d_xy = textureLod(dfdxy_image, texcoord, float(in_lvl)).xyz;

                //if (kf == kf_image_nodata || f == f_image_nodata || dfdxy.xy == dfdxy_image_nodata.xy)
                //{
                //    discard;
                //}

                float f_exp = apply_exposure(kf, exposure);
                float d_fexp_d_f = d_f_exp_d_f(kf, exposure);
                vec3 d_fexp_d_exp = d_f_exp_d_exp(kf, exposure);
                vec3 d_fexp_d_xy = d_fexp_d_f * d_f_d_xy;

                float v0 = d_fexp_d_xy.x * fx * out_width / f_ver.z;
                float v1 = d_fexp_d_xy.y * fy * out_height / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                vec3 d_f_i_d_f_ver = vec3(v0, v1, v2);
                vec3 d_f_i_d_rot = vec3(-f_ver.z * v1 + f_ver.y * v2, f_ver.z * v0 - f_ver.x * v2, -f_ver.y * v0 + f_ver.x * v1);

                float d_f_i_d_kf_depth_0 = dot(d_f_i_d_f_ver, kf_ray_0);
                float d_f_i_d_kf_depth_1 = dot(d_f_i_d_f_ver, kf_ray_1);
                float d_f_i_d_kf_depth_2 = dot(d_f_i_d_f_ver, kf_ray_2);

                vec3 d_depth_d_vert_depth = bc;

                vec3 jac;
                jac.x = d_f_i_d_kf_depth_0 * d_depth_d_vert_depth.x;
                jac.y = d_f_i_d_kf_depth_1 * d_depth_d_vert_depth.y;
                jac.z = d_f_i_d_kf_depth_2 * d_depth_d_vert_depth.z;

                jtra_output = d_f_i_d_f_ver;
                jrot_output = d_f_i_d_rot;
                jexp_output = d_fexp_d_exp;
                jmap_output = jac;
                pids_output = triIDs;
                image_output = f_exp;
            }
            )Shader";

        create_framebuffer(fbo_, rbo_);
        program_ = create_program(vertex_shader, geometry_shader, fragment_shader, common);

        view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");

        fx_loc_ = glGetUniformLocation(program_, "fx");
        fy_loc_ = glGetUniformLocation(program_, "fy");
        cx_loc_ = glGetUniformLocation(program_, "cx");
        cy_loc_ = glGetUniformLocation(program_, "cy");

        kf_image_loc_ = glGetUniformLocation(program_, "kf_image");
        kf_image_nodata_loc_ = glGetUniformLocation(program_, "kf_image_nodata");

        dfdxy_image_loc_ = glGetUniformLocation(program_, "dfdxy_image");
        dfdxy_image_nodata_loc_ = glGetUniformLocation(program_, "dfdxy_image_nodata");

        in_lvl_loc_ = glGetUniformLocation(program_, "in_lvl");
        out_lvl_loc_ = glGetUniformLocation(program_, "out_lvl");

        out_width_loc_ = glGetUniformLocation(program_, "out_width");
        out_height_loc_ = glGetUniformLocation(program_, "out_height");

        exposure_loc_ = glGetUniformLocation(program_, "exposure");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<ImageType> &kf_texture,
                const TextureGL<Vec3<float>> &dfdxy_texture,
                TextureGL<ImageType> &image_texture,
                TextureGL<Vec3<float>> &jtra_texture,
                TextureGL<Vec3<float>> &jrot_texture,
                TextureGL<Vec3<float>> &jexp_texture,
                TextureGL<Vec3<float>> &jdepth_texture,
                TextureGL<Vec3<PidType>> &pids_texture)
    {
        // save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, jtra_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, jrot_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, jexp_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, jdepth_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, pids_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, image_texture.id(), out_lvl);

        const GLenum bufs[6] = {GL_COLOR_ATTACHMENT0,
                                GL_COLOR_ATTACHMENT1,
                                GL_COLOR_ATTACHMENT2,
                                GL_COLOR_ATTACHMENT3,
                                GL_COLOR_ATTACHMENT4,
                                GL_COLOR_ATTACHMENT5};
        glDrawBuffers(6, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(image_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(image_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        Vec3<float> jtra_nodata = jtra_texture.nodata();
        Vec3<float> jrot_nodata = jrot_texture.nodata();
        Vec3<float> jexp_nodata = jexp_texture.nodata();
        Vec3<float> jdepth_nodata = jdepth_texture.nodata();
        Vec3<float> pids_nodata = pids_texture.nodata();
        float image_nodata = image_texture.nodata();

        float jtra_clear[4] = {jtra_nodata(0), jtra_nodata(1), jtra_nodata(2), 1.f};
        float jrot_clear[4] = {jrot_nodata(0), jrot_nodata(1), jrot_nodata(2), 1.f};
        float jexp_clear[4] = {jexp_nodata(0), jexp_nodata(1), jexp_nodata(2), 1.f};
        float jdepth_clear[4] = {jdepth_nodata(0), jdepth_nodata(1), jdepth_nodata(2), 1.f};
        float pids_clear[4] = {pids_nodata(0), pids_nodata(1), pids_nodata(2), 1.f};
        float image_clear[4] = {image_nodata, 0.f, 0.f, 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, jmap_clear);
        //         glClearBufferfv(GL_COLOR, 1, pids_clear);
        //         glClearBufferfv(GL_COLOR, 2, r_clear);
        // #else
        //  Clear GL_COLOR_ATTACHMENT0
        const GLenum bufs0[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs0);
        glClearColor(jtra_clear[0], jtra_clear[1], jtra_clear[2], jtra_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT1
        const GLenum bufs1[1] = {GL_COLOR_ATTACHMENT1};
        glDrawBuffers(1, bufs1);
        glClearColor(jrot_clear[0], jrot_clear[1], jrot_clear[2], jrot_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT2
        const GLenum bufs2[1] = {GL_COLOR_ATTACHMENT2};
        glDrawBuffers(1, bufs2);
        glClearColor(jexp_clear[0], jexp_clear[1], jexp_clear[2], jexp_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT3
        const GLenum bufs3[1] = {GL_COLOR_ATTACHMENT3};
        glDrawBuffers(1, bufs3);
        glClearColor(jdepth_clear[0], jdepth_clear[1], jdepth_clear[2], jdepth_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT4
        const GLenum bufs4[1] = {GL_COLOR_ATTACHMENT4};
        glDrawBuffers(1, bufs4);
        glClearColor(pids_clear[0], pids_clear[1], pids_clear[2], pids_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT5
        const GLenum bufs5[1] = {GL_COLOR_ATTACHMENT5};
        glDrawBuffers(1, bufs5);
        glClearColor(image_clear[0], image_clear[1], image_clear[2], image_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Restore glDrawBuffers for subsequent rendering.
        // This assumes the original setup was GL_COLOR_ATTACHMENT0 and GL_COLOR_ATTACHMENT1
        // as done in the clear_buffers function.
        glDrawBuffers(6, bufs);
        // #endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, kf_texture.id());
            glBindTextureUnit(1, dfdxy_texture.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, kf_texture.id());
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, dfdxy_texture.id());
        }

        glUseProgram(program_);

        const Mat4<float> view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        const Mat4<float> pose_matrix = pose.matrix();

        glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, view_matrix.data());
        glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, pose_matrix.data());

        glUniform1i(kf_image_loc_, 0);
        glUniform1f(kf_image_nodata_loc_, kf_texture.nodata());

        glUniform1i(dfdxy_image_loc_, 1);
        // glUniform3f(dfdxy_image_nodata_loc_, dfdxy_texture.nodata());

        glUniform1i(in_lvl_loc_, in_lvl);
        glUniform1i(out_lvl_loc_, out_lvl);

        glUniform1i(out_width_loc_, W);
        glUniform1i(out_height_loc_, H);

        glUniform1f(fx_loc_, cam.GetParams()(0));
        glUniform1f(fy_loc_, cam.GetParams()(1));
        glUniform1f(cx_loc_, cam.GetParams()(2));
        glUniform1f(cy_loc_, cam.GetParams()(3));

        glUniform2f(exposure_loc_, exposure(0), exposure(1));

        mesh.draw();

        // restore_state();
    }

private:
    GLuint fbo_;
    GLuint rbo_;
    GLuint program_;

    GLint view_matrix_loc_ = -1;
    GLint pose_matrix_loc_ = -1;

    GLint fx_loc_ = -1;
    GLint fy_loc_ = -1;
    GLint cx_loc_ = -1;
    GLint cy_loc_ = -1;

    GLint kf_image_loc_ = -1;
    GLint kf_image_nodata_loc_ = -1;

    GLint dfdxy_image_loc_ = -1;
    GLint dfdxy_image_nodata_loc_ = -1;

    GLint in_lvl_loc_ = -1;
    GLint out_lvl_loc_ = -1;

    GLint out_width_loc_ = -1;
    GLint out_height_loc_ = -1;

    GLint exposure_loc_ = -1;
};

class DiffRendererGL
{
public:
    DiffRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core

            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;

            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec3 v_kf_ver;
            out vec3 v_f_ver;
            out vec3 v_kf_ray;
            out vec2 v_texcoord;
            flat out int v_vertexID;

            void main() {
                vec4 ver = pose_matrix * vec4(a_position, 1.0);
                vec3 rray = mat3(pose_matrix) * a_position/a_position.z;
                gl_Position = view_matrix * ver;
                v_kf_ver = a_position;
                v_texcoord = a_texcoord;
                v_f_ver = ver.xyz;
                v_kf_ray = rray.xyz;
                v_vertexID = gl_VertexID;
            }
            )Shader";

        const char *geometry_shader = R"Shader(
            #version 330 core

            layout(triangles) in;
            layout(triangle_strip, max_vertices = 3) out;

            in vec3 v_kf_ver[];
            in vec3 v_f_ver[];
            in vec3 v_kf_ray[];
            in vec2 v_texcoord[];
            flat in int v_vertexID[];           // from VS (Option A)

            out vec3 kf_ver;
            out vec3 f_ver;
            out vec2 texcoord;
            flat out vec3 kf_ray_0;
            flat out vec3 kf_ray_1;
            flat out vec3 kf_ray_2;
            flat out ivec3 triIDs;         // to FS: the 3 vertex IDs of this triangle
            smooth out vec3  bc;           // perspective-correct barycentrics to FS
            // noperspective out vec3 bc;  // uncomment for screen-space-linear barycentrics

            // Option B: fetch element indices from a texture buffer that mirrors your EBO
            // uniform usamplerBuffer uIndexBuf;  // each texel = one uint index

            void main() {
                // Build the per-triangle ID triplet
                // Option A: use IDs passed from VS
                ivec3 ids = ivec3(v_vertexID[0], v_vertexID[1], v_vertexID[2]);

                // Option B: if using a TBO that mirrors your index buffer:
                // uint base = 3u * uint(gl_PrimitiveIDIn);
                // uvec3 ids = uvec3(
                //     texelFetch(uIndexBuf, int(base+0)).x,
                //     texelFetch(uIndexBuf, int(base+1)).x,
                //     texelFetch(uIndexBuf, int(base+2)).x
                // );

                kf_ray_0 = v_kf_ray[0];
                kf_ray_1 = v_kf_ray[1];
                kf_ray_2 = v_kf_ray[2];

                for (int i = 0; i < 3; ++i) {
                    kf_ver = v_kf_ver[i];
                    f_ver = v_f_ver[i];
                    texcoord = v_texcoord[i];
                    triIDs = ids;
                    bc     = vec3(i == 0, i == 1, i == 2);
                    gl_Position = gl_in[i].gl_Position;
                    EmitVertex();
                }
                EndPrimitive();
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core

            layout(location = 0) out vec3 jtra_output;
            layout(location = 1) out vec3 jrot_output;
            layout(location = 2) out vec3 jexp_output;
            layout(location = 3) out vec3 jmap_output;
            layout(location = 4) out vec3 pids_output;
            layout(location = 5) out float image_output;

            in vec3 kf_ver;
            in vec3 f_ver;
            in vec2 texcoord;
            flat in vec3 kf_ray_0;
            flat in vec3 kf_ray_1;
            flat in vec3 kf_ray_2;

            smooth in vec3  bc;          // or noperspective if chosen above
            flat   in ivec3 triIDs;

            uniform sampler2D kf_image;
            uniform float kf_image_nodata;

            uniform sampler2D dfdxy_image;
            uniform vec3 dfdxy_image_nodata;

            uniform int in_lvl;
            uniform int out_lvl;

            uniform int out_width;
            uniform int out_height;

            uniform float fx;
            uniform float fy;
            uniform float cx;
            uniform float cy;

            uniform vec2 exposure;

            void main()
            {
                float kf = textureLod(kf_image, texcoord, float(in_lvl)).r;
                // vec2 d_f_d_xy = texelFetch(dfdxy_image, ivec2(gl_FragCoord.x, gl_FragCoord.y), out_lvl).xy;
                vec3 d_f_d_xy = textureLod(dfdxy_image, texcoord, float(in_lvl)).xyz;

                //if (kf == kf_image_nodata || f == f_image_nodata || dfdxy.xy == dfdxy_image_nodata.xy)
                //{
                //    discard;
                //}

                float f_exp = apply_exposure(kf, exposure);
                float d_fexp_d_f = d_f_exp_d_f(kf, exposure);
                vec3 d_fexp_d_exp = d_f_exp_d_exp(kf, exposure);
                vec3 d_fexp_d_xy = d_fexp_d_f * d_f_d_xy;

                float v0 = d_fexp_d_xy.x * fx * out_width / f_ver.z;
                float v1 = d_fexp_d_xy.y * fy * out_height / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                vec3 d_f_i_d_f_ver = vec3(v0, v1, v2);
                vec3 d_f_i_d_rot = vec3(-f_ver.z * v1 + f_ver.y * v2, f_ver.z * v0 - f_ver.x * v2, -f_ver.y * v0 + f_ver.x * v1);

                float d_f_i_d_kf_depth_0 = dot(d_f_i_d_f_ver, kf_ray_0);
                float d_f_i_d_kf_depth_1 = dot(d_f_i_d_f_ver, kf_ray_1);
                float d_f_i_d_kf_depth_2 = dot(d_f_i_d_f_ver, kf_ray_2);

                vec3 d_depth_d_vert_depth = bc;

                vec3 jac;
                jac.x = d_f_i_d_kf_depth_0 * d_depth_d_vert_depth.x;
                jac.y = d_f_i_d_kf_depth_1 * d_depth_d_vert_depth.y;
                jac.z = d_f_i_d_kf_depth_2 * d_depth_d_vert_depth.z;

                jtra_output = d_f_i_d_f_ver;
                jrot_output = d_f_i_d_rot;
                jexp_output = d_fexp_d_exp;
                jmap_output = jac;
                pids_output = triIDs;
                image_output = f_exp;
            }
            )Shader";

        create_framebuffer(fbo_, rbo_);
        program_ = create_program(vertex_shader, geometry_shader, fragment_shader, common);

        view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");

        fx_loc_ = glGetUniformLocation(program_, "fx");
        fy_loc_ = glGetUniformLocation(program_, "fy");
        cx_loc_ = glGetUniformLocation(program_, "cx");
        cy_loc_ = glGetUniformLocation(program_, "cy");

        kf_image_loc_ = glGetUniformLocation(program_, "kf_image");
        kf_image_nodata_loc_ = glGetUniformLocation(program_, "kf_image_nodata");

        dfdxy_image_loc_ = glGetUniformLocation(program_, "dfdxy_image");
        dfdxy_image_nodata_loc_ = glGetUniformLocation(program_, "dfdxy_image_nodata");

        in_lvl_loc_ = glGetUniformLocation(program_, "in_lvl");
        out_lvl_loc_ = glGetUniformLocation(program_, "out_lvl");

        out_width_loc_ = glGetUniformLocation(program_, "out_width");
        out_height_loc_ = glGetUniformLocation(program_, "out_height");

        exposure_loc_ = glGetUniformLocation(program_, "exposure");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<ImageType> &diffuse_texture,
                const TextureGL<Vec3<float>> &dfdxy_texture,
                TextureGL<ImageType> &image_texture,
                TextureGL<Vec3<float>> &jtra_texture,
                TextureGL<Vec3<float>> &jrot_texture,
                TextureGL<Vec3<float>> &jexp_texture,
                TextureGL<Vec3<float>> &jmap_texture,
                TextureGL<Vec3<PidType>> &pids_texture)
    {
        // save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, jtra_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, jrot_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, jexp_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, jmap_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, pids_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, image_texture.id(), out_lvl);

        const GLenum bufs[6] = {GL_COLOR_ATTACHMENT0,
                                GL_COLOR_ATTACHMENT1,
                                GL_COLOR_ATTACHMENT2,
                                GL_COLOR_ATTACHMENT3,
                                GL_COLOR_ATTACHMENT4,
                                GL_COLOR_ATTACHMENT5};
        glDrawBuffers(6, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(image_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(image_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        Vec3<float> jtra_nodata = jtra_texture.nodata();
        Vec3<float> jrot_nodata = jrot_texture.nodata();
        Vec3<float> jexp_nodata = jexp_texture.nodata();
        Vec3<float> jmap_nodata = jmap_texture.nodata();
        Vec3<float> pids_nodata = pids_texture.nodata();
        float image_nodata = image_texture.nodata();

        float jtra_clear[4] = {jtra_nodata(0), jtra_nodata(1), jtra_nodata(2), 1.f};
        float jrot_clear[4] = {jrot_nodata(0), jrot_nodata(1), jrot_nodata(2), 1.f};
        float jexp_clear[4] = {jexp_nodata(0), jexp_nodata(1), jexp_nodata(2), 1.f};
        float jmap_clear[4] = {jmap_nodata(0), jmap_nodata(1), jmap_nodata(2), 1.f};
        float pids_clear[4] = {pids_nodata(0), pids_nodata(1), pids_nodata(2), 1.f};
        float image_clear[4] = {image_nodata, 0.f, 0.f, 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, jmap_clear);
        //         glClearBufferfv(GL_COLOR, 1, pids_clear);
        //         glClearBufferfv(GL_COLOR, 2, r_clear);
        // #else
        //  Clear GL_COLOR_ATTACHMENT0
        const GLenum bufs0[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs0);
        glClearColor(jtra_clear[0], jtra_clear[1], jtra_clear[2], jtra_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT1
        const GLenum bufs1[1] = {GL_COLOR_ATTACHMENT1};
        glDrawBuffers(1, bufs1);
        glClearColor(jrot_clear[0], jrot_clear[1], jrot_clear[2], jrot_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT2
        const GLenum bufs2[1] = {GL_COLOR_ATTACHMENT2};
        glDrawBuffers(1, bufs2);
        glClearColor(jexp_clear[0], jexp_clear[1], jexp_clear[2], jexp_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT3
        const GLenum bufs3[1] = {GL_COLOR_ATTACHMENT3};
        glDrawBuffers(1, bufs3);
        glClearColor(jmap_clear[0], jmap_clear[1], jmap_clear[2], jmap_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT4
        const GLenum bufs4[1] = {GL_COLOR_ATTACHMENT4};
        glDrawBuffers(1, bufs4);
        glClearColor(pids_clear[0], pids_clear[1], pids_clear[2], pids_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT5
        const GLenum bufs5[1] = {GL_COLOR_ATTACHMENT5};
        glDrawBuffers(1, bufs5);
        glClearColor(image_clear[0], image_clear[1], image_clear[2], image_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Restore glDrawBuffers for subsequent rendering.
        // This assumes the original setup was GL_COLOR_ATTACHMENT0 and GL_COLOR_ATTACHMENT1
        // as done in the clear_buffers function.
        glDrawBuffers(6, bufs);
        // #endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, diffuse_texture.id());
            glBindTextureUnit(1, dfdxy_texture.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, diffuse_texture.id());
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, dfdxy_texture.id());
        }

        glUseProgram(program_);

        const Mat4<float> view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE,
                                                                RenderConstants::FAR_PLANE);
        const Mat4<float> pose_matrix = pose.matrix();

        glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, view_matrix.data());
        glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, pose_matrix.data());

        glUniform1i(kf_image_loc_, 0);
        glUniform1f(kf_image_nodata_loc_, diffuse_texture.nodata());

        glUniform1i(dfdxy_image_loc_, 1);
        // glUniform3f(dfdxy_image_nodata_loc_, dfdxy_texture.nodata());

        glUniform1i(in_lvl_loc_, in_lvl);
        glUniform1i(out_lvl_loc_, out_lvl);

        glUniform1i(out_width_loc_, W);
        glUniform1i(out_height_loc_, H);

        glUniform1f(fx_loc_, cam.GetParams()(0));
        glUniform1f(fy_loc_, cam.GetParams()(1));
        glUniform1f(cx_loc_, cam.GetParams()(2));
        glUniform1f(cy_loc_, cam.GetParams()(3));

        glUniform2f(exposure_loc_, exposure(0), exposure(1));

        mesh.draw();

        // restore_state();
    }

private:
    GLuint fbo_;
    GLuint rbo_;
    GLuint program_;

    GLint view_matrix_loc_ = -1;
    GLint pose_matrix_loc_ = -1;

    GLint fx_loc_ = -1;
    GLint fy_loc_ = -1;
    GLint cx_loc_ = -1;
    GLint cy_loc_ = -1;

    GLint kf_image_loc_ = -1;
    GLint kf_image_nodata_loc_ = -1;

    GLint dfdxy_image_loc_ = -1;
    GLint dfdxy_image_nodata_loc_ = -1;

    GLint in_lvl_loc_ = -1;
    GLint out_lvl_loc_ = -1;

    GLint out_width_loc_ = -1;
    GLint out_height_loc_ = -1;

    GLint exposure_loc_ = -1;
};