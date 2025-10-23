#pragma once

#include "core/camera.h"
#include "core/render_constants.h"
#include "core/error_handling.h"

#include "backends/gles2/devicegles2_glad.h"
#include "backends/gles2/meshgles2.h"
#include "backends/gles2/texturegles2.h"

class BaseRendererGLES2
{
public:
    BaseRendererGLES2()
    {
        glGenFramebuffers(1, &fbo_);
        glGenRenderbuffers(1, &rboDepth_); // depth-only in ES2 core

        // OpenCV (image) Y-up to GL (Y-down) fix, plus your handedness tweak
        opencv2opengl_ = linalg::Mat4<float>::Identity();
        opencv2opengl_(1, 1) = -1.0f; // flip Y
        opencv2opengl_(2, 2) = -1.0f; // flip Z (if you want RH->LH)
    }

    BaseRendererGLES2(BaseRendererGLES2 &&o) noexcept { *this = std::move(o); }
    BaseRendererGLES2 &operator=(BaseRendererGLES2 &&o) noexcept
    {
        if (this != &o)
        {
            destroy_();
            fbo_ = std::exchange(o.fbo_, 0);
            rboDepth_ = std::exchange(o.rboDepth_, 0);
            program_ = std::exchange(o.program_, 0);
        }
        return *this;
    }

    BaseRendererGLES2(const BaseRendererGLES2 &) = delete;
    BaseRendererGLES2 &operator=(const BaseRendererGLES2 &) = delete;

    virtual ~BaseRendererGLES2() { destroy_(); }

protected:
    // Compile + link (ES2): bind attrib locations BEFORE link so they match MeshGLES2 (0:pos,1:nor,2:uv)
    void CompileShaders(const char *vs, const char *fs)
    {
        GLuint vsId = compile_shader_(GL_VERTEX_SHADER, vs);
        GLuint fsId = compile_shader_(GL_FRAGMENT_SHADER, fs);

        program_ = glCreateProgram();
        glAttachShader(program_, vsId);
        glAttachShader(program_, fsId);

        glBindAttribLocation(program_, 0, "aPos");
        glBindAttribLocation(program_, 1, "aNormal");
        glBindAttribLocation(program_, 2, "aTexCoords");

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
            throw RendererExceptions::OpenGLException("shader program linking", 0);
        }
    }

    // ES2 has no geometry shaders; keep an overload that throws if ever called.
    void CompileShaders(const char *, const char *, const char *)
    {
        throw RendererExceptions::OpenGLException("Geometry shaders are not available in GLES 2.0", 0);
    }

    void check_framebuffer()
    {
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            throw RendererExceptions::OpenGLException("framebuffer setup", status);
        }
    }

    void save_state()
    {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo_);
        glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg_);
        glGetIntegerv(GL_VIEWPORT, prevViewport_);
    }

    void restore_state()
    {
        glUseProgram(prevProg_);
        glBindFramebuffer(GL_FRAMEBUFFER, prevFbo_);
        glViewport(prevViewport_[0], prevViewport_[1], prevViewport_[2], prevViewport_[3]);
    }

    GLuint program_ = 0;
    GLuint fbo_ = 0;
    GLuint rboDepth_ = 0;

    GLint prevFbo_ = 0, prevProg_ = 0, prevViewport_[4] = {0, 0, 0, 0};

    linalg::Mat4<float> opencv2opengl_;

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
            glDeleteShader(id);
            throw RendererExceptions::OpenGLException(
                (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment") + std::string(" shader compilation"), ok);
        }
        return id;
    }

    void destroy_()
    {
        if (program_)
            glDeleteProgram(program_);
        if (rboDepth_)
            glDeleteRenderbuffers(1, &rboDepth_);
        if (fbo_)
            glDeleteFramebuffers(1, &fbo_);
        program_ = 0;
        fbo_ = 0;
        rboDepth_ = 0;
    }
};

// --------------------- A tiny ES2 example pass ---------------------

class SimpleExampleRendererGLES2 : public BaseRendererGLES2
{
public:
    SimpleExampleRendererGLES2()
        : BaseRendererGLES2()
    {
        // GLSL ES 1.00 shaders (attribute/varying, precision, gl_FragColor)
        const char *vertex_shader = R"GLSL(
            #version 100
            precision mediump float;

            attribute vec3 aPos;
            attribute vec3 aNormal;
            attribute vec2 aTexCoords;

            varying vec2 vTexCoords;

            uniform mat4 model;
            uniform mat4 view;
            uniform mat4 projection;

            void main() {
                vTexCoords   = aTexCoords;
                gl_Position  = projection * view * model * vec4(aPos, 1.0);
            }
        )GLSL";

        const char *fragment_shader = R"GLSL(
            #version 100
            precision mediump float;

            varying vec2 vTexCoords;

            uniform sampler2D image;
            // uniform float image_nodata; // not used in this demo
            // uniform int   image_lvl;    // ES2 can't force exact mip level anyway

            void main() {
                vec4 tex = texture2D(image, vTexCoords);
                gl_FragColor = tex;
            }
        )GLSL";

        CompileShaders(vertex_shader, fragment_shader);

        projection_loc_ = glGetUniformLocation(program_, "projection");
        view_loc_ = glGetUniformLocation(program_, "view");
        model_loc_ = glGetUniformLocation(program_, "model");
        image_loc_ = glGetUniformLocation(program_, "image");
        image_nodata_loc_ = glGetUniformLocation(program_, "image_nodata");
        image_lvl_loc_ = glGetUniformLocation(program_, "image_lvl");
    }

    void Render(const MeshGLES2 &mesh,
                linalg::Mat4<float> &projection,
                linalg::Mat4<float> &view,
                linalg::Mat4<float> &model,
                int /*in_lvl*/,
                int out_lvl,
                TextureGLES2<linalg::Vec3<float>> &in_texture,
                TextureGLES2<linalg::Vec3<float>> &out_texture)
    {
        // Validate inputs
        ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        save_state();

        // Bind our FBO and attach color target (ES2 uses *Texture2D* form)
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, out_texture.id(), out_lvl);

        // Make sure depth buffer matches the target size
        const GLsizei W = static_cast<GLsizei>(out_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(out_texture.height(out_lvl));

        glBindRenderbuffer(GL_RENDERBUFFER, rboDepth_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, W, H);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth_);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        glViewport(0, 0, W, H);

        // Clear with something reasonable; if you want nodata, adapt to your T
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Bind input texture on unit 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, in_texture.id());

        glUseProgram(program_);

        glUniformMatrix4fv(projection_loc_, 1, GL_FALSE, projection.data());
        glUniformMatrix4fv(view_loc_, 1, GL_FALSE, view.data());
        glUniformMatrix4fv(model_loc_, 1, GL_FALSE, model.data());

        glUniform1i(image_loc_, 0); // sampler -> texture unit 0
        // glUniform1f(image_nodata_loc_, ...); // if you actually use it in shader
        // glUniform1i(image_lvl_loc_, in_lvl); // ES2 can't force exact LOD

        mesh.draw();

        restore_state();
    }

private:
    GLint projection_loc_ = -1;
    GLint view_loc_ = -1;
    GLint model_loc_ = -1;

    GLint image_loc_ = -1;
    GLint image_nodata_loc_ = -1;
    GLint image_lvl_loc_ = -1;
};

class ImageRendererGLES2 : public BaseRendererGLES2
{
public:
    ImageRendererGLES2() : BaseRendererGLES2()
    {
        // GLSL ES 1.00 shaders (attribute/varying, precision, gl_FragColor)
        const char *vertex_shader = R"GLSL(
            #version 100
            precision mediump float;

            attribute vec3 a_position;
            attribute vec2 a_texcoord;

            uniform mat4 t_matrix;

            varying vec2 texcoord;

            void main() {
                gl_Position = t_matrix * vec4(a_position, 1.0);
                texcoord = a_texcoord;
            }
            )GLSL";

        const char *fragment_shader = R"GLSL(
            #version 100
            precision mediump float;

            varying vec2 texcoord;

            uniform sampler2D image;
            uniform float image_nodata;
            uniform int image_lvl;

            void main()
            {
                float val = texture2D(image, texcoord).r;

                //if (val == image_nodata)
                //    discard;
                
                gl_FragColor.r = val;
            }
            )GLSL";

        CompileShaders(vertex_shader, fragment_shader);

        t_matrix_loc_ = glGetUniformLocation(program_, "t_matrix");

        image_loc_ = glGetUniformLocation(program_, "image");
        image_nodata_loc_ = glGetUniformLocation(program_, "image_nodata");
        image_lvl_loc_ = glGetUniformLocation(program_, "image_lvl");
    }

    void Render(MeshGLES2 &mesh,
                const linalg::SE3<float> &pose,
                const Camera<float> &cam,
                int in_lvl,
                int out_lvl,
                TextureGLES2<float> &out_texture)
    {
        save_state();

        // Bind our FBO and attach color target (ES2 uses *Texture2D* form)
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, out_texture.id(), out_lvl);

        // Make sure depth buffer matches the target size
        const GLsizei W = static_cast<GLsizei>(out_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(out_texture.height(out_lvl));

        glBindRenderbuffer(GL_RENDERBUFFER, rboDepth_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, W, H);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth_);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glDisable(GL_SCISSOR_TEST);
        // this is because of the flip in y in the output image
        // so that we can display using opencv
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        glViewport(0, 0, W, H);

        float clear[4] = {out_texture.nodata(), 0.f, 0.f, 1.f};

        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mesh.diffuse().id());

        glUseProgram(program_);

        const linalg::Mat4<float> t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) *
                                             opencv2opengl_ *
                                             pose.matrix();
        glUniformMatrix4fv(t_matrix_loc_, 1, GL_FALSE, t_matrix.data());

        glUniform1i(image_loc_, 0);                              // texture unit
        glUniform1f(image_nodata_loc_, mesh.diffuse().nodata()); // **int**, not float
        glUniform1i(image_lvl_loc_, in_lvl);                     // **int**, not float

        mesh.draw();

        restore_state();
    }

private:
    GLint t_matrix_loc_ = -1;

    GLint image_loc_;
    GLint image_nodata_loc_;
    GLint image_lvl_loc_;
};