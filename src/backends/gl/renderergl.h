#pragma once
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <cassert>

#include "backends/gl/devicegl_glad.h"
#include "core/format_converters.h"
#include "core/camera.h"
#include "backends/gl/meshgl.h"
#include "backends/gl/texturegl.h"

template <typename InTexType, typename OutTexType>
class BaseRendererGL
{
public:
    BaseRendererGL()
    {
#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glCreateFramebuffers(1, &fbo_);
        }
        else
#endif
        {
            glGenFramebuffers(1, &fbo_);
        }
    }

    // move-only RAII
    BaseRendererGL(BaseRendererGL &&o) noexcept { *this = std::move(o); }
    BaseRendererGL &operator=(BaseRendererGL &&o) noexcept
    {
        if (this != &o)
        {
            destroy_();
            fbo_ = std::exchange(o.fbo_, 0);
            program_ = std::exchange(o.program_, 0);
            view_matrix_loc_ = o.view_matrix_loc_;
            pose_matrix_loc_ = o.pose_matrix_loc_;
            fx_loc_ = o.fx_loc_;
            fy_loc_ = o.fy_loc_;
            image_loc_ = o.image_loc_;
            image_nodata_loc_ = o.image_nodata_loc_;
            image_lvl_loc_ = o.image_lvl_loc_;
        }
        return *this;
    }
    BaseRendererGL(const BaseRendererGL &) = delete;
    BaseRendererGL &operator=(const BaseRendererGL &) = delete;

    virtual ~BaseRendererGL() { destroy_(); }

    // Derived classes call this once after constructing to compile & link
    void CompileShaders(const char *vs, const char *fs)
    {
        GLuint vsId = compile_shader_(GL_VERTEX_SHADER, vs);
        GLuint fsId = compile_shader_(GL_FRAGMENT_SHADER, fs);

        program_ = glCreateProgram();
        glAttachShader(program_, vsId);
        glAttachShader(program_, fsId);
        glLinkProgram(program_);
        glDeleteShader(vsId);
        glDeleteShader(fsId);

        GLint ok = GL_FALSE;
        glGetProgramiv(program_, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            char log[2048];
            glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
            std::cerr << "Program link failed:\n"
                      << log << "\n";
        }

        // Common uniform locations (derived shaders should use these names)
        view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");
        fx_loc_ = glGetUniformLocation(program_, "fx");
        fy_loc_ = glGetUniformLocation(program_, "fy");
        image_loc_ = glGetUniformLocation(program_, "image");
        image_nodata_loc_ = glGetUniformLocation(program_, "image_nodata");
        image_lvl_loc_ = glGetUniformLocation(program_, "image_lvl");
    }

    // Main draw
    void Render(const MeshGL &mesh,
                const SE3 &pose,
                const Camera &cam,
                const TextureGL<InTexType> &texture_in,
                TextureGL<OutTexType> &texture_out,
                int in_lvl,
                int out_lvl)
    {
        // ——— Save a bit of state we touch ———
        GLint prevFbo = 0, prevProg = 0, prevViewport[4];
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
        glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
        glGetIntegerv(GL_VIEWPORT, prevViewport);

        // ——— Attach output level to our FBO ———
#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
            glNamedFramebufferTexture(fbo_, GL_COLOR_ATTACHMENT0, texture_out.id(), out_lvl);
        }
        else
#endif
        {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_out.id(), out_lvl);
        }

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "FBO incomplete (0x" << std::hex << status << std::dec << ")\n";
            // restore minimal state
            glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
            glUseProgram(prevProg);
            glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
            return;
        }

        // ——— Fixed pipeline state for our pass ———
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);

        const GLsizei W = static_cast<GLsizei>(texture_out.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(texture_out.height(out_lvl));
        glViewport(0, 0, W, H);

        // If you want to clear to "nodata", map OutTexType to RGBA here.
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        // ——— Bind input texture on unit 0 ———
#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, texture_in.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture_in.id());
        }

        // ——— Program + uniforms ———
        glUseProgram(program_);

        // Matrices (column-major order, transpose = GL_FALSE)
        Mat4 opencv2opengl = Mat4::Identity();
        opencv2opengl(2, 2) = -1.0; // flip Z; (1,1) was already +1

        const Mat4 view_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl;
        const Mat4 pose_matrix = pose.matrix();

        GLfloat viewArr[16], poseArr[16];
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
            {
                // column-major packing
                viewArr[c * 4 + r] = static_cast<GLfloat>(view_matrix(r, c));
                poseArr[c * 4 + r] = static_cast<GLfloat>(pose_matrix(r, c));
            }

        if (view_matrix_loc_ >= 0)
            glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, viewArr);
        if (pose_matrix_loc_ >= 0)
            glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, poseArr);

        if (fx_loc_ >= 0)
            glUniform1f(fx_loc_, static_cast<GLfloat>(cam.GetParams()(0)));
        if (fy_loc_ >= 0)
            glUniform1f(fy_loc_, static_cast<GLfloat>(cam.GetParams()(1)));

        if (image_loc_ >= 0)
            glUniform1i(image_loc_, 0); // texture unit
        if (image_lvl_loc_ >= 0)
            glUniform1i(image_lvl_loc_, in_lvl); // **int**, not float

        // If you want to pass nodata for masking, set image_nodata_loc_ here based on InTexType.

        // ——— Draw ———
        mesh.bind();
        mesh.draw();
        mesh.unbind();

        // ——— Restore previous state ———
        glUseProgram(prevProg);
        glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    }

protected:
    GLuint program_ = 0;
    GLuint fbo_ = 0;

    // common uniform locations (optional to use in derived shaders)
    GLint view_matrix_loc_ = -1;
    GLint pose_matrix_loc_ = -1;
    GLint fx_loc_ = -1;
    GLint fy_loc_ = -1;
    GLint image_loc_ = -1;
    GLint image_nodata_loc_ = -1;
    GLint image_lvl_loc_ = -1;

private:
    static GLuint compile_shader_(GLenum type, const char *src)
    {
        GLuint id = glCreateShader(type);
        glShaderSource(id, 1, &src, nullptr);
        glCompileShader(id);
        GLint ok = GL_FALSE;
        glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[2048];
            glGetShaderInfoLog(id, sizeof(log), nullptr, log);
            std::cerr << (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment")
                      << " shader compilation failed:\n"
                      << log << "\n";
        }
        return id;
    }

    void destroy_()
    {
        if (program_)
            glDeleteProgram(program_);
        if (fbo_)
            glDeleteFramebuffers(1, &fbo_);
        program_ = 0;
        fbo_ = 0;
    }
};

class DepthRendererGL : public BaseRendererGL<float /*InTexType*/, float /*OutTexType*/>
{
public:
    DepthRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in float a_weight;
            
            out float depth;
            
            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            void main() {
                gl_Position = (view_matrix * pose_matrix) * vec4(a_position, 1.0);
                depth = a_position.z;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out float a_output;
            in float depth;

            //uniform sampler2D image;
            //uniform float image_nodata;
            //uniform int image_lvl;

            void main()
            {
                a_output = depth;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
    }

private:
};

class ImageRendererGL : public BaseRendererGL<float /*InTexType*/, float /*OutTexType*/>
{
public:
    ImageRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in float a_weight;
            
            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec2 texcoord;

            void main() {
                gl_Position = view_matrix * pose_matrix * vec4(a_position, 1.0);
                texcoord = a_texcoord;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out float a_output;
            in vec2 texcoord;

            uniform sampler2D image;
            uniform float image_nodata;
            uniform int image_lvl;

            void main()
            {
                a_output = textureLod(image, texcoord, float(image_lvl)).r;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
    }

private:
};

class DIDxyRendererGL : public BaseRendererGL<float /*InTexType*/, Vec3 /*OutTexType*/>
{
public:
    DIDxyRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in float a_weight;
            
            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec2 texcoord;

            void main() {
                // gl_Position = view_matrix * pose_matrix * vec4(a_position, 1.0);
                gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
                texcoord = a_texcoord;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out vec3 a_output;
            in vec2 texcoord;

            uniform sampler2D image;
            uniform float image_nodata;
            uniform int image_lvl;

            void main()
            {
                ivec2 tex_size = textureSize(image, image_lvl);
                int x = int(texcoord.x * (tex_size.x - 1));
                int y = int(texcoord.y * (tex_size.y - 1));
                int x_p = x + 1;
                int x_m = x - 1;
                int y_p = y + 1;
                int y_m = y - 1;

                if (x_p >= tex_size.x || x_m < 0 || y_p >= tex_size.y || y_m < 0)
                {
                    //no need to explicitly set to nodata, it is already in the background color
                    //a_output = nodata;
                    discard;
                }

                float f_x_p = texelFetch(image, ivec2(x_p, y), image_lvl).r;
                float f_x_m = texelFetch(image, ivec2(x_m, y), image_lvl).r;
                float f_y_p = texelFetch(image, ivec2(x, y_p), image_lvl).r;
                float f_y_m = texelFetch(image, ivec2(x, y_m), image_lvl).r;

                if (f_x_p == image_nodata || f_x_m == image_nodata ||
                    f_y_p == image_nodata || f_y_m == image_nodata)
                {
                    //no need to explicitly set to nodata, it is already in the background color
                    //a_output = nodata;
                    discard;
                }

                a_output.x = (f_x_p - f_x_m) / 2.0f;
                a_output.y = (f_y_p - f_y_m) / 2.0f;
                a_output.z = 0.0f;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
    }

private:
};

class JtraRendererGL : public BaseRendererGL<Vec3 /*InTexType*/, Vec3 /*OutTexType*/>
{
public:
    JtraRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in float a_weight;
            
            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec2 texcoord;
            out vec3 f_ver;
            //flat out float fx;
            //flat out float fy;

            void main() {
                vec4 ver = pose_matrix * vec4(a_position, 1.0);
                gl_Position = view_matrix * ver;

                f_ver = ver.xyz;
                //fx = view_matrix[0][0] / 2.0f;
                //fy = view_matrix[1][1] / 2.0f;
                
                // Pass texture coordinates to fragment shader
                texcoord = a_texcoord;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out vec3 a_output;
            
            in vec2 texcoord;
            in vec3 f_ver;
            //flat in float fx;
            //flat in float fy;

            uniform sampler2D image;
            uniform vec3 image_nodata;
            uniform int image_lvl;

            uniform float fx;
            uniform float fy;

            void main()
            {
                ivec2 tex_size = textureSize(image, image_lvl);

                //vec3 didxy = texture(image, texcoord).xyz;
                vec3 didxy = textureLod(image, texcoord, float(image_lvl)).xyz;
                //vec3 didxy = vec3(image_lvl, image_lvl, image_lvl);

                //if(didxy == image_nodata)
                //    discard;
                if (didxy == vec3(0.0f, 0.0f, 0.0f))
                {
                    didxy = vec3(1.0f, 1.0f, 1.0f);
                }

                float v0 = didxy.x * fx * tex_size.x / f_ver.z;
                float v1 = didxy.y * fy * tex_size.y / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                a_output = vec3(v0, v1, v2);
                //vec3f d_f_i_d_rot(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
    }

private:
};

class JrotRendererGL : public BaseRendererGL<Vec3 /*InTexType*/, Vec3 /*OutTexType*/>
{
public:
    JrotRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in float a_weight;
            
            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec2 texcoord;
            out vec3 f_ver;
            //flat out float fx;
            //flat out float fy;

            void main() {
                vec4 ver = pose_matrix * vec4(a_position, 1.0);
                gl_Position = view_matrix * ver;

                f_ver = ver.xyz;
                //fx = view_matrix[0][0] / 2.0f;
                //fy = view_matrix[1][1] / 2.0f;
                
                // Pass texture coordinates to fragment shader
                texcoord = a_texcoord;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out vec3 a_output;
            
            in vec2 texcoord;
            in vec3 f_ver;
            //flat in float fx;
            //flat in float fy;

            uniform sampler2D image;
            uniform vec3 image_nodata;
            uniform int image_lvl;

            void main()
            {
                //vec3 v = texture(image, texcoord).xyz;
                vec3 v = textureLod(image, texcoord, float(image_lvl)).xyz;

                if(v == image_nodata)
                    discard;

                //float v0 = didxy.x * fx / f_ver.z;
                //float v1 = didxy.y * fy / f_ver.z;
                //float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                //a_output = vec3f(v0, v1, v2);
                a_output = vec3(-f_ver.z * v.y + f_ver.x * v.z, f_ver.z * v.x - f_ver.x * v.z, -f_ver.y * v.x + f_ver.x * v.y);

            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
    }

private:
};

class JPoseRendererGL : public BaseRendererGL<Vec2 /*InTexType*/, float /*OutTexType*/>
{
public:
    JPoseRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in float a_weight;
            
            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec2 texcoord;

            void main() {
                f_ver = pose_matrix * vec4(a_position, 1.0);
                gl_Position = view_matrix * f_ver;
                texcoord = a_texcoord;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out float a_output;
            in vec2 texcoord;

            uniform sampler2D image;
            uniform float image_nodata;
            uniform int image_lvl;

            void main()
            {
                vec3 v = texture(image, texcoord).r;

                float v0 = f_der.x * fx * id;
                float v1 = f_der.y * fy * id;
                float v2 = -(v0 * pframe.x + v1 * pframe.y) * id;

                vec3f d_f_i_d_tra = vec3f(v0, v1, v2);
                vec3f d_f_i_d_rot(-f_ver(2) * v(1) + f_ver(1) * v(2), f_ver(2) * v(0) - f_ver(0) * v(2), -f_ver(1) * v(0) + f_ver(0) * v(1));

            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
    }

private:
};