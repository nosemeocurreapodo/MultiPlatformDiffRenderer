#pragma once
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <type_traits>
#include <cassert>

#include "backends/gl/devicegl_glad.h"
#include "core/format_converters.h"
#include "core/types.h"
#include "core/camera.h"
#include "backends/gl/meshgl.h"
#include "backends/gl/texturegl.h"

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

        opencv2opengl_ = Mat4::Identity();
        opencv2opengl_(2, 2) = -1.0; // flip Z; (1,1) was already +1
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
            // view_matrix_loc_ = o.view_matrix_loc_;
            // pose_matrix_loc_ = o.pose_matrix_loc_;
            // fx_loc_ = o.fx_loc_;
            // fy_loc_ = o.fy_loc_;
            // image_loc_ = o.image_loc_;
            // image_nodata_loc_ = o.image_nodata_loc_;
            // image_lvl_loc_ = o.image_lvl_loc_;
        }
        return *this;
    }
    BaseRendererGL(const BaseRendererGL &) = delete;
    BaseRendererGL &operator=(const BaseRendererGL &) = delete;

    virtual ~BaseRendererGL() { destroy_(); }

protected:
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
            glDeleteProgram(program_);
            program_ = 0;
            throw std::runtime_error(std::string("Program link failed:\n") + log);
        }

        // Common uniform locations (derived shaders should use these names)
        // view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        // pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");
        // fx_loc_ = glGetUniformLocation(program_, "fx");
        // fy_loc_ = glGetUniformLocation(program_, "fy");
        // image_loc_ = glGetUniformLocation(program_, "image");
        // image_nodata_loc_ = glGetUniformLocation(program_, "image_nodata");
        // image_lvl_loc_ = glGetUniformLocation(program_, "image_lvl");
    }

    void CompileShaders(const char *vs, const char *gs, const char *fs)
    {
        GLuint vsId = compile_shader_(GL_VERTEX_SHADER, vs);
        GLuint gsId = compile_shader_(GL_GEOMETRY_SHADER, gs);
        GLuint fsId = compile_shader_(GL_FRAGMENT_SHADER, fs);

        program_ = glCreateProgram();
        glAttachShader(program_, vsId);
        glAttachShader(program_, gsId);
        glAttachShader(program_, fsId);
        glLinkProgram(program_);
        glDeleteShader(vsId);
        glDeleteShader(gsId);
        glDeleteShader(fsId);

        GLint ok = GL_FALSE;
        glGetProgramiv(program_, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            char log[2048];
            glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
            glDeleteProgram(program_);
            program_ = 0;
            throw std::runtime_error(std::string("Program link failed:\n") + log);
        }

        // Common uniform locations (derived shaders should use these names)
        // view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        // pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");
        // fx_loc_ = glGetUniformLocation(program_, "fx");
        // fy_loc_ = glGetUniformLocation(program_, "fy");
        // image_loc_ = glGetUniformLocation(program_, "image");
        // image_nodata_loc_ = glGetUniformLocation(program_, "image_nodata");
        // image_lvl_loc_ = glGetUniformLocation(program_, "image_lvl");
    }

    void check_framebuffer()
    {
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            // restore minimal state
            // glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
            // glUseProgram(prevProg);
            // glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
            throw std::runtime_error("Framebuffer is not complete!");
        }
    }

    void save_state()
    {
        GLint prevFbo = 0, prevProg = 0, prevViewport[4];
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
        glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
        glGetIntegerv(GL_VIEWPORT, prevViewport);
    }

    void restore_state()
    {
        glUseProgram(prevProg_);
        glBindFramebuffer(GL_FRAMEBUFFER, prevFbo_);
        glViewport(prevViewport_[0], prevViewport_[1], prevViewport_[2], prevViewport_[3]);
    }

    GLuint program_ = 0;
    GLuint fbo_ = 0;

    GLint prevFbo_ = 0, prevProg_ = 0, prevViewport_[4];

    Mat4 opencv2opengl_;

    // common uniform locations (optional to use in derived shaders)
    // GLint view_matrix_loc_ = -1;
    // GLint pose_matrix_loc_ = -1;
    // GLint fx_loc_ = -1;
    // GLint fy_loc_ = -1;
    // GLint image_loc_ = -1;
    // GLint image_nodata_loc_ = -1;
    // GLint image_lvl_loc_ = -1;

    // Derived &derived() { return *static_cast<Derived *>(this); }
    // const Derived &derived() const { return *static_cast<const Derived *>(this); }

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
            std::string shader_type = (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment");
            glDeleteShader(id);
            throw std::runtime_error(shader_type + " shader compilation failed:\n" + log);
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

class DepthRendererGL : public BaseRendererGL
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
            
            uniform mat4 t_matrix;

            void main() {
                gl_Position = t_matrix * vec4(a_position, 1.0);
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

        t_matrix_loc_ = glGetUniformLocation(program_, "t_matrix");
    }

    void Render(const MeshGL &mesh,
                const SE3 &pose,
                const Camera &cam,
                int out_lvl,
                TextureGL<float> &depth_texture)
    {
        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, depth_texture.id(), out_lvl);

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        check_framebuffer();

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);

        const GLsizei W = static_cast<GLsizei>(depth_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(depth_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        float clear[4] = {depth_texture.nodata(), 0.f, 0.f, 1.f};

#if defined(GL_VERSION_3_0)
        glClearBufferfv(GL_COLOR, 0, clear);
#else
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
#endif

        glUseProgram(program_);
        // set_uniforms();

        const Mat4 t_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl_ * pose.matrix();
        glUniformMatrix4fv(t_matrix_loc_, 1, GL_FALSE, t_matrix.data());

        mesh.bind();
        mesh.draw();
        mesh.unbind();

        restore_state();
    }

private:
    GLint t_matrix_loc_ = -1;
};

class ImageRendererGL : public BaseRendererGL
{
public:
    ImageRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in float a_weight;
            
            uniform mat4 t_matrix;

            out vec2 texcoord;

            void main() {
                gl_Position = t_matrix * vec4(a_position, 1.0);
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
                float val = textureLod(image, texcoord, float(image_lvl)).r;
                if (val == image_nodata)
                    discard;
                a_output = val;
            )
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);

        t_matrix_loc_ = glGetUniformLocation(program_, "t_matrix");

        image_loc_ = glGetUniformLocation(program_, "image");
        image_nodata_loc_ = glGetUniformLocation(program_, "image_nodata");
        image_lvl_loc_ = glGetUniformLocation(program_, "image_lvl");
    }

    void Render(const MeshGL &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<float> &in_texture,
                TextureGL<float> &out_texture)
    {
        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, out_texture.id(), out_lvl);

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        check_framebuffer();

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);

        const GLsizei W = static_cast<GLsizei>(out_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(out_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        float clear[4] = {out_texture.nodata(), 0.f, 0.f, 1.f};

#if defined(GL_VERSION_3_0)
        glClearBufferfv(GL_COLOR, 0, clear);
#else
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
#endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, in_texture.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, in_texture.id());
        }

        glUseProgram(program_);
        // set_uniforms();

        const Mat4 t_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl_ * pose.matrix();
        glUniformMatrix4fv(t_matrix_loc_, 1, GL_FALSE, t_matrix.data());

        glUniform1i(image_loc_, 0);                          // texture unit
        glUniform1f(image_nodata_loc_, in_texture.nodata()); // **int**, not float
        glUniform1i(image_lvl_loc_, in_lvl);                 // **int**, not float

        mesh.bind();
        mesh.draw();
        mesh.unbind();

        restore_state();
    }

private:
    GLint t_matrix_loc_ = -1;

    GLint image_loc_;
    GLint image_nodata_loc_;
    GLint image_lvl_loc_;
};

class ResidualRendererGL : public BaseRendererGL
{
public:
    ResidualRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in float a_weight;
            
            uniform mat4 t_matrix;

            out vec2 texcoord;

            void main() {
                gl_Position = t_matrix * vec4(a_position, 1.0);
                texcoord = a_texcoord;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out float a_output;
            in vec2 texcoord;

            uniform sampler2D kf_image;
            uniform sampler2D f_image;
            uniform float kf_image_nodata;
            uniform float f_image_nodata;
            uniform int kf_image_lvl;
            uniform int f_image_lvl;

            void main()
            {
                float kf = textureLod(kf_image, texcoord, float(kf_image_lvl)).r;
                float f = textureLod(f_image, texcoord, float(f_image_lvl)).r;

                if (kf == kf_image_nodata || f == f_image_nodata)
                    discard;

                float r = f - kf;

                a_output = r;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);

        t_matrix_loc_ = glGetUniformLocation(program_, "t_matrix");

        kf_image_loc_ = glGetUniformLocation(program_, "kf_image");
        kf_image_nodata_loc_ = glGetUniformLocation(program_, "kf_image_nodata");
        kf_image_lvl_loc_ = glGetUniformLocation(program_, "kf_image_lvl");

        f_image_loc_ = glGetUniformLocation(program_, "f_image");
        f_image_nodata_loc_ = glGetUniformLocation(program_, "f_image_nodata");
        f_image_lvl_loc_ = glGetUniformLocation(program_, "f_image_lvl");
    }

    void Render(const MeshGL &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<float> &kf_texture,
                const TextureGL<float> &f_texture,
                TextureGL<float> &r_texture)
    {
        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, r_texture.id(), out_lvl);

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        check_framebuffer();

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);

        const GLsizei W = static_cast<GLsizei>(r_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(r_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        float clear[4] = {r_texture.nodata(), 0.f, 0.f, 1.f};

#if defined(GL_VERSION_3_0)
        glClearBufferfv(GL_COLOR, 0, clear);
#else
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
#endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, kf_texture.id());
            glBindTextureUnit(1, f_texture.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, kf_texture.id());
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, f_texture.id());
        }

        glUseProgram(program_);

        const Mat4 t_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl_ * pose.matrix();
        glUniformMatrix4fv(t_matrix_loc_, 1, GL_FALSE, t_matrix.data());

        glUniform1i(kf_image_loc_, 0);                          // texture unit
        glUniform1f(kf_image_nodata_loc_, kf_texture.nodata()); // **int**, not float
        glUniform1i(kf_image_lvl_loc_, in_lvl);                 // **int**, not float

        glUniform1i(f_image_loc_, 0);                         // texture unit
        glUniform1f(f_image_nodata_loc_, f_texture.nodata()); // **int**, not float
        glUniform1i(f_image_lvl_loc_, in_lvl);                // **int**, not float

        mesh.bind();
        mesh.draw();
        mesh.unbind();

        restore_state();
    }

private:
    GLint t_matrix_loc_ = -1;

    GLint kf_image_loc_;
    GLint kf_image_nodata_loc_;
    GLint kf_image_lvl_loc_;

    GLint f_image_loc_;
    GLint f_image_nodata_loc_;
    GLint f_image_lvl_loc_;
};

class L2RendererGL : public BaseRendererGL
{
public:
    L2RendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in float a_weight;
            
            uniform mat4 t_matrix;

            out vec2 texcoord;

            void main() {
                gl_Position = t_matrix * vec4(a_position, 1.0);
                texcoord = a_texcoord;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out float a_output;
            in vec2 texcoord;

            uniform sampler2D kf_image;
            uniform sampler2D f_image;
            uniform float kf_image_nodata;
            uniform float f_image_nodata;
            uniform int kf_image_lvl;
            uniform int f_image_lvl;

            void main()
            {
                float kf = textureLod(kf_image, texcoord, float(kf_image_lvl)).r;
                float f = textureLod(f_image, texcoord, float(f_image_lvl)).r;

                if (kf == kf_image_nodata || f == f_image_nodata)
                    discard;

                float r = f - kf;

                a_output = r * r;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);

        t_matrix_loc_ = glGetUniformLocation(program_, "t_matrix");

        kf_image_loc_ = glGetUniformLocation(program_, "kf_image");
        kf_image_nodata_loc_ = glGetUniformLocation(program_, "kf_image_nodata");
        kf_image_lvl_loc_ = glGetUniformLocation(program_, "kf_image_lvl");

        f_image_loc_ = glGetUniformLocation(program_, "f_image");
        f_image_nodata_loc_ = glGetUniformLocation(program_, "f_image_nodata");
        f_image_lvl_loc_ = glGetUniformLocation(program_, "f_image_lvl");
    }

    void Render(const MeshGL &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<float> &kf_texture,
                const TextureGL<float> &f_texture,
                TextureGL<float> &r_texture)
    {
        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, r_texture.id(), out_lvl);

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        check_framebuffer();

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);

        const GLsizei W = static_cast<GLsizei>(r_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(r_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        float clear[4] = {r_texture.nodata(), 0.f, 0.f, 1.f};

#if defined(GL_VERSION_3_0)
        glClearBufferfv(GL_COLOR, 0, clear);
#else
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
#endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, kf_texture.id());
            glBindTextureUnit(1, f_texture.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, kf_texture.id());
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, f_texture.id());
        }

        glUseProgram(program_);

        const Mat4 t_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl_ * pose.matrix();
        glUniformMatrix4fv(t_matrix_loc_, 1, GL_FALSE, t_matrix.data());

        glUniform1i(kf_image_loc_, 0);                          // texture unit
        glUniform1f(kf_image_nodata_loc_, kf_texture.nodata()); // **int**, not float
        glUniform1i(kf_image_lvl_loc_, in_lvl);                 // **int**, not float

        glUniform1i(f_image_loc_, 0);                         // texture unit
        glUniform1f(f_image_nodata_loc_, f_texture.nodata()); // **int**, not float
        glUniform1i(f_image_lvl_loc_, in_lvl);                // **int**, not float

        mesh.bind();
        mesh.draw();
        mesh.unbind();

        restore_state();
    }

private:
    GLint t_matrix_loc_ = -1;

    GLint kf_image_loc_;
    GLint kf_image_nodata_loc_;
    GLint kf_image_lvl_loc_;

    GLint f_image_loc_;
    GLint f_image_nodata_loc_;
    GLint f_image_lvl_loc_;
};

class DIDxyRendererGL : public BaseRendererGL
{
public:
    DIDxyRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in float a_weight;
            
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

        image_loc_ = glGetUniformLocation(program_, "image");
        image_nodata_loc_ = glGetUniformLocation(program_, "image_nodata");
        image_lvl_loc_ = glGetUniformLocation(program_, "image_lvl");
    }

    void Render(const MeshGL &mesh,
                int in_lvl,
                int out_lvl,
                const TextureGL<float> &in_texture,
                TextureGL<Vec3> &out_texture)
    {
        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, out_texture.id(), out_lvl);

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        check_framebuffer();

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);

        const GLsizei W = static_cast<GLsizei>(out_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(out_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        Vec3 nodata = out_texture.nodata();
        float clear[4] = {nodata(0), nodata(1), nodata(2), 1.f};

#if defined(GL_VERSION_3_0)
        glClearBufferfv(GL_COLOR, 0, clear);
#else
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
#endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, in_texture.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, in_texture.id());
        }

        glUseProgram(program_);

        glUniform1i(image_loc_, 0);                          // texture unit
        glUniform1f(image_nodata_loc_, in_texture.nodata()); // **int**, not float
        glUniform1i(image_lvl_loc_, in_lvl);                 // **int**, not float

        mesh.bind();
        mesh.draw();
        mesh.unbind();

        restore_state();
    }

private:
    GLint image_loc_;
    GLint image_nodata_loc_;
    GLint image_lvl_loc_;
};

class JPoseRendererGL : public BaseRendererGL
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

            out vec3 f_ver;
            out vec2 texcoord;

            void main() {
                f_ver = pose_matrix * vec4(a_position, 1.0);
                gl_Position = view_matrix * f_ver;
                texcoord = a_texcoord;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out vec3 jtra_output;
            layout(location = 1) out vec3 jrot_output;
            layout(location = 2) out vec3 r_output;

            in vec3 f_ver;
            in vec2 texcoord;

            uniform sampler2D kf_image;
            uniform float kf_image_nodata;
            uniform int kf_image_lvl;

            uniform sampler2D f_image;
            uniform float f_image_nodata;
            uniform int f_image_lvl;

            uniform sampler2D dfdxy_image;
            uniform vec3 dfdxy_image_nodata;
            uniform int dfdxy_image_lvl;

            uniform float fx;
            uniform float fy;

            void main()
            {
                ivec2 tex_size = textureSize(image, image_lvl);

                float kf = texture(kf_image, texcoord).r;
                float f = texture(f_image, texcoord).r;
                vec2 dfdxy = texture(dfdxy_image, texcoord).rg;

                if (kf == kf_image_nodata || f == f_image_nodata || dfdxy.xy == dfdxy_image_nodata.xy)
                {
                    discard;
                }

                float r = f - kf;
                
                float v0 = dfdxy.x * fx * tex_size.x / f_ver.z;
                float v1 = dfdxy.y * fy * tex_size.y / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                vec3f d_f_i_d_tra = vec3f(v0, v1, v2);
                vec3f d_f_i_d_rot(-f_ver(2) * v(1) + f_ver(1) * v(2), f_ver(2) * v(0) - f_ver(0) * v(2), -f_ver(1) * v(0) + f_ver(0) * v(1));

                jtra_output = d_f_i_d_tra;
                jrot_output = d_f_i_d_rot;
                r_output = r;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);

        view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");

        kf_image_loc_ = glGetUniformLocation(program_, "kf_image");
        kf_image_nodata_loc_ = glGetUniformLocation(program_, "kf_image_nodata");
        kf_image_lvl_loc_ = glGetUniformLocation(program_, "kf_image_lvl");

        f_image_loc_ = glGetUniformLocation(program_, "f_image");
        f_image_nodata_loc_ = glGetUniformLocation(program_, "f_image_nodata");
        f_image_lvl_loc_ = glGetUniformLocation(program_, "f_image_lvl");

        dfdxy_image_loc_ = glGetUniformLocation(program_, "dfdxy_image");
        dfdxy_image_nodata_loc_ = glGetUniformLocation(program_, "dfdxy_image_nodata");
        dfdxy_image_lvl_loc_ = glGetUniformLocation(program_, "dfdxy_image_lvl");
    }

    void Render(const MeshGL &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<float> &kf_texture,
                const TextureGL<float> &f_texture,
                const TextureGL<Vec3> &dfdxy_texture,
                TextureGL<Vec3> &jtra_texture,
                TextureGL<Vec3> &jrot_texture,
                TextureGL<float> &r_texture)
    {
        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, jtra_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, jrot_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, r_texture.id(), out_lvl);

        const GLenum bufs[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
        glDrawBuffers(3, bufs);

        check_framebuffer();

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);

        const GLsizei W = static_cast<GLsizei>(jtra_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(jtra_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        Vec3 jtra_nodata = jtra_texture.nodata();
        Vec3 jrot_nodata = jrot_texture.nodata();
        float r_nodata = r_texture.nodata();

        float jtra_clear[4] = {jtra_nodata(0), jtra_nodata(1), jtra_nodata(2), 1.f};
        float jrot_clear[4] = {jrot_nodata(0), jrot_nodata(1), jrot_nodata(2), 1.f};
        float r_clear[4] = {r_nodata, 0.f, 0.f, 1.f};

#if defined(GL_VERSION_3_0)
        glClearBufferfv(GL_COLOR, 0, jtra_clear);
        glClearBufferfv(GL_COLOR, 1, jrot_clear);
        glClearBufferfv(GL_COLOR, 2, r_clear);
#else
        // Clear GL_COLOR_ATTACHMENT0
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
        glClearColor(r_clear[0], r_clear[1], r_clear[2], r_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Restore glDrawBuffers for subsequent rendering.
        // This assumes the original setup was GL_COLOR_ATTACHMENT0 and GL_COLOR_ATTACHMENT1
        // as done in the clear_buffers function.
        const GLenum bufs_restore[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
        glDrawBuffers(2, bufs_restore);
#endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, kf_texture.id());
            glBindTextureUnit(1, f_texture.id());
            glBindTextureUnit(1, dfdxy_texture.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, kf_texture.id());
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, f_texture.id());
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, dfdxy_texture.id());
        }

        glUseProgram(program_);

        const Mat4 view_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl_;
        const Mat4 pose_matrix = pose.matrix();

        glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, view_matrix.data());
        glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, pose_matrix.data());

        glUniform1i(kf_image_loc_, 0);
        glUniform1f(kf_image_nodata_loc_, kf_texture.nodata());
        glUniform1i(kf_image_lvl_loc_, in_lvl);

        glUniform1i(f_image_loc_, 0);
        glUniform1f(f_image_nodata_loc_, f_texture.nodata());
        glUniform1i(f_image_lvl_loc_, in_lvl);

        glUniform1i(dfdxy_image_loc_, 0);
        glUniform1f(dfdxy_image_nodata_loc_, f_texture.nodata());
        glUniform1i(dfdxy_image_lvl_loc_, in_lvl);

        mesh.bind();
        mesh.draw();
        mesh.unbind();

        restore_state();
    }

private:
    GLint view_matrix_loc_ = -1;
    GLint pose_matrix_loc_ = -1;

    GLint kf_image_loc_ = -1;
    GLint kf_image_nodata_loc_ = -1;
    GLint kf_image_lvl_loc_ = -1;

    GLint f_image_loc_ = -1;
    GLint f_image_nodata_loc_ = -1;
    GLint f_image_lvl_loc_ = -1;

    GLint dfdxy_image_loc_ = -1;
    GLint dfdxy_image_nodata_loc_ = -1;
    GLint dfdxy_image_lvl_loc_ = -1;
};

class JMapRendererGL : public BaseRendererGL
{
public:
    JMapRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in float a_weight;
            //layout (location = 3) in uint a_VertexID;

            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec3 v_f_ver;
            out vec3 v_kf_ray;
            out vec2 v_texcoord;
            flat out uint v_vertexID;

            void main() {
                f_ver = pose_matrix * vec4(a_position, 1.0);
                kf_ray = mat3(pose_matrix) * a_position/a_position.z;
                gl_Position = view_matrix * f_ver;
                texcoord = a_texcoord;
                v_vertexID = gl_VertexID;
            }
            )Shader";

        const char *geometry_shader = R"Shader(
            #version 330 core
            layout(triangles) in;
            layout(triangle_strip, max_vertices = 3) out;

            in vec3 v_f_ver[];
            in vec3 v_kf_ray[];
            in vec2 v_texcoord[];
            flat in  uint v_vertexID[];           // from VS (Option A)

            out vec3 f_ver;
            out vec3 kf_ray;
            out vec2 texcoord;
            flat out uvec3 triIDs;         // to FS: the 3 vertex IDs of this triangle
            smooth out vec3  bc;           // perspective-correct barycentrics to FS
            // noperspective out vec3 bc;  // uncomment for screen-space-linear barycentrics

            // Option B: fetch element indices from a texture buffer that mirrors your EBO
            // uniform usamplerBuffer uIndexBuf;  // each texel = one uint index

            void main() {
                // Build the per-triangle ID triplet
                // Option A: use IDs passed from VS
                uvec3 ids = uvec3(v_vertexID[0], v_vertexID[1], v_vertexID[2]);

                // Option B: if using a TBO that mirrors your index buffer:
                // uint base = 3u * uint(gl_PrimitiveIDIn);
                // uvec3 ids = uvec3(
                //     texelFetch(uIndexBuf, int(base+0)).x,
                //     texelFetch(uIndexBuf, int(base+1)).x,
                //     texelFetch(uIndexBuf, int(base+2)).x
                // );

                for (int i = 0; i < 3; ++i) {
                    f_ver = v_f_ver[i];
                    kf_ray = v_kf_ray[i];
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
            layout(location = 0) out vec3 jmap_output;
            layout(location = 1) out vec3i pid_output;
            layout(location = 2) out float r_output;

            in vec3 f_ver;
            in vec3 kf_ray;
            in vec2 texcoord;

            smooth in vec3  bc;          // or noperspective if chosen above
            flat   in uvec3 triIDs;

            uniform sampler2D kf_image;
            uniform float kf_image_nodata;
            uniform int kf_image_lvl;

            uniform sampler2D f_image;
            uniform float f_image_nodata;
            uniform int f_image_lvl;

            uniform sampler2D dfdxy_image;
            uniform float dfdxy_image_nodata;
            uniform int dfdxy_image_lvl;

            void main()
            {
                float kf = texture(kf_image, texcoord).r;
                float f = texture(f_image, texcoord).r;
                vec2 dfdxy = texture(dfdxy_image, texcoord).rg;

                if (kf == kf_image_nodata || f == f_image_nodata || dfdxy.xy == dfdxy_image_nodata.xy)
                {
                    discard;
                }

                float r = f - kf;

                float v0 = dfdxy.x * fx * tex_size.x / f_ver.z;
                float v1 = dfdxy.y * fy * tex_size.y / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                vec3f d_f_i_d_f_ver = vec3f(v0, v1, v2);
                //vec3f d_f_i_d_rot(-f_ver(2) * v(1) + f_ver(1) * v(2), f_ver(2) * v(0) - f_ver(0) * v(2), -f_ver(1) * v(0) + f_ver(0) * v(1));

                //jtra_output = d_f_i_d_tra;
                //jrot_output = d_f_i_d_rot;

                Vec3 d_f_ver_d_kf_depth = kf_ray;
                float d_f_i_d_kf_depth = d_f_i_d_f_ver.transpose() * d_f_ver_d_kf_depth;

                Vec3 d_depth_d_vert_depth = bc;

                Vec3 jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;

                jmap_texture.set_texel_(jac, gl_FragCoord(1), gl_FragCoord(0), out_lvl);
                pid_texture.set_texel_(triIDs, gl_FragCoord(1), gl_FragCoord(0), out_lvl);
                r_texture.set_texel_(r, gl_FragCoord(1), gl_FragCoord(0), out_lvl);
            }
            )Shader";

        CompileShaders(vertex_shader, geometry_shader, fragment_shader);

        view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");

        kf_image_loc_ = glGetUniformLocation(program_, "kf_image");
        kf_image_nodata_loc_ = glGetUniformLocation(program_, "kf_image_nodata");
        kf_image_lvl_loc_ = glGetUniformLocation(program_, "kf_image_lvl");

        f_image_loc_ = glGetUniformLocation(program_, "f_image");
        f_image_nodata_loc_ = glGetUniformLocation(program_, "f_image_nodata");
        f_image_lvl_loc_ = glGetUniformLocation(program_, "f_image_lvl");

        dfdxy_image_loc_ = glGetUniformLocation(program_, "dfdxy_image");
        dfdxy_image_nodata_loc_ = glGetUniformLocation(program_, "dfdxy_image_nodata");
        dfdxy_image_lvl_loc_ = glGetUniformLocation(program_, "dfdxy_image_lvl");
    }

    void Render(const MeshGL &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<float> &kf_texture,
                const TextureGL<float> &f_texture,
                const TextureGL<Vec3> &dfdxy_texture,
                TextureGL<Vec3> &jmap_texture,
                TextureGL<Vec3i> &pids_texture,
                TextureGL<float> &r_texture)
    {
        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, jmap_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, pids_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, r_texture.id(), out_lvl);

        const GLenum bufs[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
        glDrawBuffers(3, bufs);

        check_framebuffer();

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);

        const GLsizei W = static_cast<GLsizei>(jmap_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(jmap_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        Vec3 jmap_nodata = jmap_texture.nodata();
        Vec3i pids_nodata = pids_texture.nodata();
        float r_nodata = r_texture.nodata();

        float jmap_clear[4] = {jmap_nodata(0), jmap_nodata(1), jmap_nodata(2), 1.f};
        int pids_clear[4] = {pids_nodata(0), pids_nodata(1), pids_nodata(2), 1};
        float r_clear[4] = {r_nodata, 0.f, 0.f, 1.f};

#if defined(GL_VERSION_3_0)
        glClearBufferfv(GL_COLOR, 0, jmap_clear);
        glClearBufferiv(GL_COLOR, 1, pids_clear);
        glClearBufferfv(GL_COLOR, 2, r_clear);
#else
        // Clear GL_COLOR_ATTACHMENT0
        const GLenum bufs0[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs0);
        glClearColor(jmap_clear[0], jmap_clear[1], jmap_clear[2], jmap_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT1
        const GLenum bufs1[1] = {GL_COLOR_ATTACHMENT1};
        glDrawBuffers(1, bufs1);
        glClearColor(pids_clear[0], pids_clear[1], pids_clear[2], pids_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT2
        const GLenum bufs2[1] = {GL_COLOR_ATTACHMENT2};
        glDrawBuffers(1, bufs2);
        glClearColor(r_clear[0], r_clear[1], r_clear[2], r_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Restore glDrawBuffers for subsequent rendering.
        // This assumes the original setup was GL_COLOR_ATTACHMENT0 and GL_COLOR_ATTACHMENT1
        // as done in the clear_buffers function.
        const GLenum bufs_restore[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
        glDrawBuffers(2, bufs_restore);
#endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, kf_texture.id());
            glBindTextureUnit(1, f_texture.id());
            glBindTextureUnit(1, dfdxy_texture.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, kf_texture.id());
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, f_texture.id());
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, dfdxy_texture.id());
        }

        glUseProgram(program_);

        const Mat4 view_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl_;
        const Mat4 pose_matrix = pose.matrix();

        glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, view_matrix.data());
        glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, pose_matrix.data());

        glUniform1i(kf_image_loc_, 0);
        glUniform1f(kf_image_nodata_loc_, kf_texture.nodata());
        glUniform1i(kf_image_lvl_loc_, in_lvl);

        glUniform1i(f_image_loc_, 0);
        glUniform1f(f_image_nodata_loc_, f_texture.nodata());
        glUniform1i(f_image_lvl_loc_, in_lvl);

        glUniform1i(dfdxy_image_loc_, 0);
        glUniform1f(dfdxy_image_nodata_loc_, f_texture.nodata());
        glUniform1i(dfdxy_image_lvl_loc_, in_lvl);

        mesh.bind();
        mesh.draw();
        mesh.unbind();

        restore_state();
    }

private:
    GLint view_matrix_loc_ = -1;
    GLint pose_matrix_loc_ = -1;

    GLint kf_image_loc_ = -1;
    GLint kf_image_nodata_loc_ = -1;
    GLint kf_image_lvl_loc_ = -1;

    GLint f_image_loc_ = -1;
    GLint f_image_nodata_loc_ = -1;
    GLint f_image_lvl_loc_ = -1;

    GLint dfdxy_image_loc_ = -1;
    GLint dfdxy_image_nodata_loc_ = -1;
    GLint dfdxy_image_lvl_loc_ = -1;
};