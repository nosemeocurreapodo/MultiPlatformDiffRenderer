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

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glGenRenderbuffers(1, &rbo_);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1280, 1280); // use a single renderbuffer object for both a depth AND stencil buffer.

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        opencv2opengl_ = Mat4<float>::Identity();
        opencv2opengl_(1, 1) = -1.0;
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
            throw RendererExceptions::OpenGLException("shader program linking", 0);
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
            throw RendererExceptions::OpenGLException("shader program linking", 0);
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
            throw RendererExceptions::OpenGLException("framebuffer setup", status);
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
    GLuint rbo_;

    GLint prevFbo_ = 0, prevProg_ = 0, prevViewport_[4];

    Mat4<float> opencv2opengl_;

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
            std::cout << log << std::endl;
            throw RendererExceptions::OpenGLException(shader_type + " shader compilation", ok);
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

class SimpleExampleRendererGL : public BaseRendererGL
{
public:
    SimpleExampleRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec3 aNormal;
            layout (location = 2) in vec2 aTexCoords;

            out vec2 TexCoords;

            uniform mat4 model;
            uniform mat4 view;
            uniform mat4 projection;

            void main()
            {
                TexCoords = aTexCoords;    
                gl_Position = projection * view * model * vec4(aPos, 1.0);
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            out vec3 FragColor;

            in vec2 TexCoords;

            uniform sampler2D image;
            uniform float image_nodata;
            uniform int image_lvl;

            void main()
            {    
                FragColor = texture(image, TexCoords).xyz;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);

        projection_loc_ = glGetUniformLocation(program_, "projection");
        view_loc_ = glGetUniformLocation(program_, "view");
        model_loc_ = glGetUniformLocation(program_, "model");

        image_loc_ = glGetUniformLocation(program_, "image");
        image_nodata_loc_ = glGetUniformLocation(program_, "image_nodata");
        image_lvl_loc_ = glGetUniformLocation(program_, "image_lvl");
    }

    void Render(const MeshGL &mesh,
                Mat4<float> &projection,
                Mat4<float> &view,
                Mat4<float> &model,
                int in_lvl,
                int out_lvl,
                TextureGL<Vec3<float>> &in_texture,
                TextureGL<Vec3<float>> &out_texture)
    {
        // Validate inputs
        ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, out_texture.id(), out_lvl);

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);

        const GLsizei W = static_cast<GLsizei>(out_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(out_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        float clear[4] = {out_texture.nodata()(0), out_texture.nodata()(1), out_texture.nodata()(2), 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, clear);
        // #else
        // glClearColor(clear[0], clear[1], clear[2], clear[3]);
        // glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // #endif

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, in_texture.id());

        glUseProgram(program_);

        glUniformMatrix4fv(projection_loc_, 1, GL_FALSE, projection.data());
        glUniformMatrix4fv(view_loc_, 1, GL_FALSE, view.data());
        glUniformMatrix4fv(model_loc_, 1, GL_FALSE, model.data());

        glUniform1i(image_loc_, 0); // texture unit
        // glUniform1f(image_nodata_loc_, in_texture.nodata()); // **int**, not float
        glUniform1i(image_lvl_loc_, in_lvl); // **int**, not float

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

class GouraudRendererGL : public BaseRendererGL
{
public:
    GouraudRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core

            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in vec3 aNormal;

            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;
            uniform mat3 uNormalMatrix;   // transpose(inverse(mat3(uModel))) computed on CPU

            uniform vec3 uLightPos;       // world-space
            uniform vec3 uViewPos;        // camera position in world-space

            // Material and light
            uniform vec3 uKa;             // ambient reflectance (rgb)
            uniform vec3 uKd;             // diffuse reflectance (rgb)
            uniform vec3 uKs;             // specular reflectance (rgb)
            uniform float uShininess;     // specular exponent
            uniform vec3 uLightColor;     // light color/intensity (rgb)
            uniform vec3 uAmbientLight;   // ambient light (rgb)

            out vec3 vColor;              // lit color computed per-vertex

            void main() {
                // Transform to world space
                vec3 fragPos = vec3(uModel * vec4(aPos, 1.0));
                vec3 N = normalize(uNormalMatrix * aNormal);

                // Lighting vectors
                vec3 L = normalize(uLightPos - fragPos);
                vec3 V = normalize(uViewPos  - fragPos);
                float n_dot_l = dot(N, L);
                //vec3 R = reflect(L, N);
                vec3 R = L - 2.0f * n_dot_l * N; // opengls reflect

                // Phong reflectance model (computed per-vertex)
                float NdotL = max(n_dot_l, 0.0);
                float spec = 0.0;
                if (NdotL > 0.0) {
                    spec = pow(max(dot(V, R), 0.0), uShininess);
                }

                vec3 ambient  = uAmbientLight * uKa;
                vec3 diffuse  = uLightColor * uKd * NdotL;
                vec3 specular = uLightColor * uKs * spec;

                vColor = ambient + diffuse + specular;

                gl_Position = uProjection * uView * vec4(fragPos, 1.0);
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core

            in vec3 vColor;
            out vec4 FragColor;

            void main() {
                FragColor = vec4(vColor, 1.0);
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);

        uModel_loc_ = glGetUniformLocation(program_, "uModel");
        uView_loc_ = glGetUniformLocation(program_, "uView");
        uProjection_loc_ = glGetUniformLocation(program_, "uProjection");
        uNormalMatrix_loc_ = glGetUniformLocation(program_, "uNormalMatrix");
        uLightPos_loc_ = glGetUniformLocation(program_, "uLightPos");
        uViewPos_loc_ = glGetUniformLocation(program_, "uViewPos");
        uKa_loc_ = glGetUniformLocation(program_, "uKa");
        uKd_loc_ = glGetUniformLocation(program_, "uKd");
        uKs_loc_ = glGetUniformLocation(program_, "uKs");
        uShininess_loc_ = glGetUniformLocation(program_, "uShininess");
        uLightColor_loc_ = glGetUniformLocation(program_, "uLightColor");
        uAmbientLight_loc_ = glGetUniformLocation(program_, "uAmbientLight");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                const Vec3<float> &light_pos,
                const Vec3<float> &light_color,
                const Vec3<float> &ambient_reflectance,
                const Vec3<float> &diffuse_reflectance,
                const Vec3<float> &specular_reflectance,
                const float shininess,
                const Vec3<float> &ambient_light,
                unsigned int out_lvl,
                TextureGL<Vec3<float>> &out_texture)
    {
        // Validate inputs
        ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, out_texture.id(), out_lvl);

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(out_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(out_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        float clear[4] = {out_texture.nodata()(0), out_texture.nodata()(1), out_texture.nodata()(2), 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, clear);
        // #else
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // #endif

        glUseProgram(program_);

        SE3<float> cam2world = pose.inverse();

        // model already in world space
        Mat4<float> uModel = Mat4<float>::Identity();
        Mat4<float> uView = this->opencv2opengl_ * pose.matrix();
        Mat4<float> uProjection = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        Mat3<float> uNormalMatrix = Mat3<float>::Identity(); // Mat3<MathType>(uModel_).inverse().transpose();

        Vec3<float> uViewPos = cam2world.translation(); // camera position in world space

        glUniformMatrix4fv(uModel_loc_, 1, GL_FALSE, uModel.data());
        glUniformMatrix4fv(uView_loc_, 1, GL_FALSE, uView.data());
        glUniformMatrix4fv(uProjection_loc_, 1, GL_FALSE, uProjection.data());
        glUniformMatrix3fv(uNormalMatrix_loc_, 1, GL_FALSE, uNormalMatrix.data());

        glUniform3fv(uLightPos_loc_, 1, light_pos.data());
        glUniform3fv(uViewPos_loc_, 1, uViewPos.data());

        glUniform3fv(uKa_loc_, 1, ambient_reflectance.data());
        glUniform3fv(uKd_loc_, 1, diffuse_reflectance.data());
        glUniform3fv(uKs_loc_, 1, specular_reflectance.data());

        glUniform1fv(uShininess_loc_, 1, &shininess);

        glUniform3fv(uLightColor_loc_, 1, light_color.data());
        glUniform3fv(uAmbientLight_loc_, 1, ambient_light.data());

        mesh.draw();

        restore_state();
    }

private:
    GLint uModel_loc_ = -1;
    GLint uView_loc_ = -1;
    GLint uProjection_loc_ = -1;
    GLint uNormalMatrix_loc_ = -1;
    GLint uLightPos_loc_ = -1;
    GLint uViewPos_loc_ = -1;
    GLint uKa_loc_ = -1;
    GLint uKd_loc_ = -1;
    GLint uKs_loc_ = -1;
    GLint uShininess_loc_ = -1;
    GLint uLightColor_loc_ = -1;
    GLint uAmbientLight_loc_ = -1;
};

class DepthRendererGL : public BaseRendererGL
{
public:
    DepthRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            
            out float depth;
            
            uniform mat4 t_matrix;

            void main() {
                vec4 t_position = t_matrix * vec4(a_position, 1.0);
                gl_Position = t_position;
                depth = t_position.z;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out float a_output;

            in float depth;

            uniform float near_plane;
            uniform float far_plane;
            
            //uniform sampler2D image;
            //uniform float image_nodata;
            //uniform int image_lvl;

            void main()
            {
                a_output = depth;
                //a_output = (far_plane - near_plane) * (0.5 * gl_FragCoord.z + 0.5) + near_plane;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);

        t_matrix_loc_ = glGetUniformLocation(program_, "t_matrix");
        near_plane_loc_ = glGetUniformLocation(program_, "near_plane");
        far_plane_loc_ = glGetUniformLocation(program_, "far_plane");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int out_lvl,
                TextureGL<float> &depth_texture)
    {
        // Validate inputs
        ErrorHandling::ValidateTextureDimensions(depth_texture.width(out_lvl), depth_texture.height(out_lvl), out_lvl);
        ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, depth_texture.id(), out_lvl);

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(depth_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(depth_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        float clear[4] = {depth_texture.nodata(), 0.f, 0.f, 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, clear);
        // #else
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // #endif

        glUseProgram(program_);

        const Mat4<float> t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) *
                                     opencv2opengl_ *
                                     pose.matrix();
        // const Mat4<float> t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * pose.matrix();

        glUniformMatrix4fv(t_matrix_loc_, 1, GL_FALSE, t_matrix.data());
        glUniform1f(near_plane_loc_, RenderConstants::NEAR_PLANE);
        glUniform1f(far_plane_loc_, RenderConstants::FAR_PLANE);

        mesh.draw();

        restore_state();
    }

private:
    GLint t_matrix_loc_ = -1;
    GLint near_plane_loc_ = -1;
    GLint far_plane_loc_ = -1;
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
                //float val = texture(image, texcoord).r;

                //if (val == image_nodata)
                //    discard;
                a_output = val;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);

        t_matrix_loc_ = glGetUniformLocation(program_, "t_matrix");

        image_loc_ = glGetUniformLocation(program_, "image");
        image_nodata_loc_ = glGetUniformLocation(program_, "image_nodata");
        image_lvl_loc_ = glGetUniformLocation(program_, "image_lvl");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<ImageType> &diffuse_texture,
                TextureGL<ImageType> &out_texture)
    {
        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, out_texture.id(), out_lvl);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_); // now actually attach it

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glDisable(GL_SCISSOR_TEST);
        // this is because of the flip in y in the output image
        // so that we can display using opencv
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(out_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(out_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        float clear[4] = {float(out_texture.nodata()), 0.f, 0.f, 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, clear);
        // #else
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // #endif

        // #if defined(GL_VERSION_4_5)
        //         if (GLAD_GL_VERSION_4_5)
        //         {
        //             glBindTextureUnit(0, in_texture.id());
        //         }
        //         else
        // #endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, diffuse_texture.id());
        }

        glUseProgram(program_);

        const Mat4<float> t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) *
                                     opencv2opengl_ *
                                     pose.matrix();
        glUniformMatrix4fv(t_matrix_loc_, 1, GL_FALSE, t_matrix.data());

        glUniform1i(image_loc_, 0);                               // texture unit
        glUniform1f(image_nodata_loc_, diffuse_texture.nodata()); // **int**, not float
        glUniform1i(image_lvl_loc_, in_lvl);                      // **int**, not float

        mesh.draw();

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
                float f = texelFetch(f_image, ivec2(gl_FragCoord.x, gl_FragCoord.y), f_image_lvl).r;

                //if (kf == kf_image_nodata || f == f_image_nodata)
                //    discard;

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
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<ImageType> &kf_texture,
                const TextureGL<ImageType> &f_texture,
                TextureGL<float> &r_texture)
    {
        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, r_texture.id(), out_lvl);

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(r_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(r_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        float clear[4] = {r_texture.nodata(), 0.f, 0.f, 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, clear);
        // #else
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // #endif

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

        const Mat4<float> t_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl_ * pose.matrix();
        glUniformMatrix4fv(t_matrix_loc_, 1, GL_FALSE, t_matrix.data());

        glUniform1i(kf_image_loc_, 0);
        glUniform1f(kf_image_nodata_loc_, kf_texture.nodata());
        glUniform1i(kf_image_lvl_loc_, in_lvl);

        glUniform1i(f_image_loc_, 1);
        glUniform1f(f_image_nodata_loc_, f_texture.nodata());
        glUniform1i(f_image_lvl_loc_, out_lvl);

        mesh.draw();

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
            
            out vec2 texcoord;

            void main() {
                // gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
                gl_Position = vec4(2.0f * a_texcoord.x - 1.0f, 2.0f * a_texcoord.y - 1.0f, 0.0f, 1.0f);
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

                //if (f_x_p == image_nodata || f_x_m == image_nodata ||
                //    f_y_p == image_nodata || f_y_m == image_nodata)
                //{
                //    //no need to explicitly set to nodata, it is already in the background color
                //    //a_output = nodata;
                //    discard;
                //}

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
                const TextureGL<ImageType> &in_texture,
                TextureGL<Vec3<float>> &out_texture)
    {
        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, out_texture.id(), out_lvl);

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(out_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(out_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        Vec3<float> nodata = out_texture.nodata();
        float clear[4] = {nodata(0), nodata(1), nodata(2), 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, clear);
        // #else
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // #endif

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

        mesh.draw();

        restore_state();
    }

private:
    GLint image_loc_;
    GLint image_nodata_loc_;
    GLint image_lvl_loc_;
};

class DIDexpRendererGL : public BaseRendererGL
{
public:
    DIDexpRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            
            out vec2 texcoord;

            void main() {
                // gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
                gl_Position = vec4(2.0f * a_texcoord.x - 1.0f, 2.0f * a_texcoord.y - 1.0f, 0.0f, 1.0f);
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
            uniform vec2 exposure;

            void main()
            {
                ivec2 tex_size = textureSize(image, image_lvl);
                int x = int(texcoord.x * (tex_size.x - 1));
                int y = int(texcoord.y * (tex_size.y - 1));

                float f = texelFetch(image, ivec2(x, y), image_lvl).r;

                a_output.x = f * exp(exposure.x);
                a_output.y = 1.0f;
                a_output.z = 0.0f;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);

        image_loc_ = glGetUniformLocation(program_, "image");
        image_nodata_loc_ = glGetUniformLocation(program_, "image_nodata");
        image_lvl_loc_ = glGetUniformLocation(program_, "image_lvl");
        exposure_loc_ = glGetUniformLocation(program_, "exposure");
    }

    void Render(const MeshGL &mesh,
                const Vec2<float> &exposure,
                int in_lvl,
                int out_lvl,
                const TextureGL<ImageType> &in_texture,
                TextureGL<Vec3<float>> &out_texture)
    {
        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, out_texture.id(), out_lvl);

        const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(out_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(out_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        Vec3<float> nodata = out_texture.nodata();
        float clear[4] = {nodata(0), nodata(1), nodata(2), 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, clear);
        // #else
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // #endif

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

        glUniform1i(image_loc_, 0);                           // texture unit
        glUniform1f(image_nodata_loc_, in_texture.nodata());  // **int**, not float
        glUniform1i(image_lvl_loc_, in_lvl);                  // **int**, not float
        glUniform2f(exposure_loc_, exposure(0), exposure(1)); // **int**, not float

        mesh.draw();

        restore_state();
    }

private:
    GLint image_loc_;
    GLint image_nodata_loc_;
    GLint image_lvl_loc_;
    GLint exposure_loc_;
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
            
            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec3 f_ver;
            out vec2 texcoord;

            void main() {
                vec4 ver = pose_matrix * vec4(a_position, 1.0);
                gl_Position = view_matrix * ver;
                f_ver = ver.xyz;
                texcoord = a_texcoord;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out vec3 jtra_output;
            layout(location = 1) out vec3 jrot_output;
            layout(location = 2) out float r_output;

            in vec3 f_ver;
            in vec2 texcoord;

            uniform sampler2D kf_image;
            uniform float kf_image_nodata;

            uniform sampler2D f_image;
            uniform float f_image_nodata;

            uniform sampler2D dfdxy_image;
            uniform vec3 dfdxy_image_nodata;
            
            uniform int in_lvl;
            uniform int out_lvl;

            uniform float fx;
            uniform float fy;

            uniform int out_width;
            uniform int out_height;

            void main()
            {
                // ivec2 tex_size = textureSize(f_image, in_lvl);

                float kf = textureLod(kf_image, texcoord, float(in_lvl)).r;
                float f = texelFetch(f_image, ivec2(gl_FragCoord.x, gl_FragCoord.y), out_lvl).r;
                vec2 dfdxy = texelFetch(dfdxy_image, ivec2(gl_FragCoord.x, gl_FragCoord.y), out_lvl).xy;

                //if (kf == kf_image_nodata || f == f_image_nodata || dfdxy.xy == dfdxy_image_nodata.xy)
                //{
                //    discard;
                //}

                float r = f - kf;
                
                float v0 = dfdxy.x * fx * out_width / f_ver.z;
                float v1 = dfdxy.y * fy * out_height / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                vec3 d_f_i_d_tra = vec3(v0, v1, v2);
                vec3 d_f_i_d_rot = vec3(-f_ver.z * v1 + f_ver.y * v2, f_ver.z * v0 - f_ver.x * v2, -f_ver.y * v0 + f_ver.x * v1);

                jtra_output = d_f_i_d_tra;
                jrot_output = d_f_i_d_rot;
                r_output = r;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);

        view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");

        fx_loc_ = glGetUniformLocation(program_, "fx");
        fy_loc_ = glGetUniformLocation(program_, "fy");

        kf_image_loc_ = glGetUniformLocation(program_, "kf_image");
        kf_image_nodata_loc_ = glGetUniformLocation(program_, "kf_image_nodata");

        f_image_loc_ = glGetUniformLocation(program_, "f_image");
        f_image_nodata_loc_ = glGetUniformLocation(program_, "f_image_nodata");

        dfdxy_image_loc_ = glGetUniformLocation(program_, "dfdxy_image");
        dfdxy_image_nodata_loc_ = glGetUniformLocation(program_, "dfdxy_image_nodata");

        in_lvl_loc_ = glGetUniformLocation(program_, "in_lvl");
        out_lvl_loc_ = glGetUniformLocation(program_, "out_lvl");

        out_width_loc_ = glGetUniformLocation(program_, "out_width");
        out_height_loc_ = glGetUniformLocation(program_, "out_height");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<ImageType> &kf_texture,
                const TextureGL<ImageType> &f_texture,
                const TextureGL<Vec3<float>> &dfdxy_texture,
                TextureGL<Vec3<float>> &jtra_texture,
                TextureGL<Vec3<float>> &jrot_texture,
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
        float r_nodata = r_texture.nodata();

        float jtra_clear[4] = {jtra_nodata(0), jtra_nodata(1), jtra_nodata(2), 1.f};
        float jrot_clear[4] = {jrot_nodata(0), jrot_nodata(1), jrot_nodata(2), 1.f};
        float r_clear[4] = {r_nodata, 0.f, 0.f, 1.f};

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
        glClearColor(r_clear[0], r_clear[1], r_clear[2], r_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Restore glDrawBuffers for subsequent rendering.
        // This assumes the original setup was GL_COLOR_ATTACHMENT0 and GL_COLOR_ATTACHMENT1
        // as done in the clear_buffers function.
        const GLenum bufs_restore[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
        glDrawBuffers(3, bufs_restore);
        // #endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, kf_texture.id());
            glBindTextureUnit(1, f_texture.id());
            glBindTextureUnit(2, dfdxy_texture.id());
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

        const Mat4<float> view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl_;
        const Mat4<float> pose_matrix = pose.matrix();

        glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, view_matrix.data());
        glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, pose_matrix.data());

        glUniform1i(kf_image_loc_, 0);
        glUniform1f(kf_image_nodata_loc_, kf_texture.nodata());

        glUniform1i(f_image_loc_, 1);
        glUniform1f(f_image_nodata_loc_, f_texture.nodata());

        glUniform1i(dfdxy_image_loc_, 2);
        // glUniform1f(dfdxy_image_nodata_loc_, dfdxy_texture.nodata());

        glUniform1i(in_lvl_loc_, in_lvl);
        glUniform1i(out_lvl_loc_, out_lvl);

        glUniform1i(out_width_loc_, W);
        glUniform1i(out_height_loc_, H);

        glUniform1f(fx_loc_, cam.GetParams()(0));
        glUniform1f(fy_loc_, cam.GetParams()(1));

        mesh.draw();

        restore_state();
    }

private:
    GLint view_matrix_loc_ = -1;
    GLint pose_matrix_loc_ = -1;

    GLint fx_loc_ = -1;
    GLint fy_loc_ = -1;

    GLint kf_image_loc_ = -1;
    GLint kf_image_nodata_loc_ = -1;

    GLint f_image_loc_ = -1;
    GLint f_image_nodata_loc_ = -1;

    GLint dfdxy_image_loc_ = -1;
    GLint dfdxy_image_nodata_loc_ = -1;

    GLint in_lvl_loc_ = -1;
    GLint out_lvl_loc_ = -1;

    GLint out_width_loc_ = -1;
    GLint out_height_loc_ = -1;
};

class JPoseExpRendererGL : public BaseRendererGL
{
public:
    JPoseExpRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            
            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec3 f_ver;
            out vec2 texcoord;

            void main() {
                vec4 ver = pose_matrix * vec4(a_position, 1.0);
                gl_Position = view_matrix * ver;
                f_ver = ver.xyz;
                texcoord = a_texcoord;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out vec3 jtra_output;
            layout(location = 1) out vec3 jrot_output;
            layout(location = 2) out vec3 jexp_output;
            layout(location = 3) out float r_output;

            in vec3 f_ver;
            in vec2 texcoord;

            uniform sampler2D kf_image;
            uniform float kf_image_nodata;

            uniform sampler2D f_image;
            uniform float f_image_nodata;

            uniform sampler2D dfdxy_image;
            uniform vec3 dfdxy_image_nodata;
            
            uniform int in_lvl;
            uniform int out_lvl;

            uniform float fx;
            uniform float fy;

            uniform int out_width;
            uniform int out_height;

            void main()
            {
                // ivec2 tex_size = textureSize(f_image, in_lvl);

                float kf = textureLod(kf_image, texcoord, float(in_lvl)).r;
                float f = texelFetch(f_image, ivec2(gl_FragCoord.x, gl_FragCoord.y), out_lvl).r;
                vec2 dfdxy = texelFetch(dfdxy_image, ivec2(gl_FragCoord.x, gl_FragCoord.y), out_lvl).xy;

                //if (kf == kf_image_nodata || f == f_image_nodata || dfdxy.xy == dfdxy_image_nodata.xy)
                //{
                //    discard;
                //}

                float r = f - kf;
                
                float v0 = dfdxy.x * fx * out_width / f_ver.z;
                float v1 = dfdxy.y * fy * out_height / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                vec3 d_f_i_d_tra = vec3(v0, v1, v2);
                vec3 d_f_i_d_rot = vec3(-f_ver.z * v1 + f_ver.y * v2, f_ver.z * v0 - f_ver.x * v2, -f_ver.y * v0 + f_ver.x * v1);

                jtra_output = d_f_i_d_tra;
                jrot_output = d_f_i_d_rot;
                jexp_out = d_f_i_d_exp;
                r_output = r;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);

        view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");

        fx_loc_ = glGetUniformLocation(program_, "fx");
        fy_loc_ = glGetUniformLocation(program_, "fy");

        kf_image_loc_ = glGetUniformLocation(program_, "kf_image");
        kf_image_nodata_loc_ = glGetUniformLocation(program_, "kf_image_nodata");

        f_image_loc_ = glGetUniformLocation(program_, "f_image");
        f_image_nodata_loc_ = glGetUniformLocation(program_, "f_image_nodata");

        dfdxy_image_loc_ = glGetUniformLocation(program_, "dfdxy_image");
        dfdxy_image_nodata_loc_ = glGetUniformLocation(program_, "dfdxy_image_nodata");

        in_lvl_loc_ = glGetUniformLocation(program_, "in_lvl");
        out_lvl_loc_ = glGetUniformLocation(program_, "out_lvl");

        out_width_loc_ = glGetUniformLocation(program_, "out_width");
        out_height_loc_ = glGetUniformLocation(program_, "out_height");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const Vec2<float> &exposure,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<ImageType> &kf_texture,
                const TextureGL<ImageType> &f_texture,
                const TextureGL<Vec3<float>> &dfdxy_texture,
                TextureGL<Vec3<float>> &jtra_texture,
                TextureGL<Vec3<float>> &jrot_texture,
                TextureGL<Vec3<float>> &jexp_texture,
                TextureGL<float> &r_texture)
    {
        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, jtra_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, jrot_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, jexp_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, r_texture.id(), out_lvl);

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
        float r_nodata = r_texture.nodata();

        float jtra_clear[4] = {jtra_nodata(0), jtra_nodata(1), jtra_nodata(2), 1.f};
        float jrot_clear[4] = {jrot_nodata(0), jrot_nodata(1), jrot_nodata(2), 1.f};
        float jexp_clear[4] = {jexp_nodata(0), jexp_nodata(1), jexp_nodata(2), 1.f};
        float r_clear[4] = {r_nodata, 0.f, 0.f, 1.f};

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
        glClearColor(r_clear[0], r_clear[1], r_clear[2], r_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
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
            glBindTextureUnit(1, f_texture.id());
            glBindTextureUnit(2, dfdxy_texture.id());
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

        const Mat4<float> view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl_;
        const Mat4<float> pose_matrix = pose.matrix();

        glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, view_matrix.data());
        glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, pose_matrix.data());

        glUniform1i(kf_image_loc_, 0);
        glUniform1f(kf_image_nodata_loc_, kf_texture.nodata());

        glUniform1i(f_image_loc_, 1);
        glUniform1f(f_image_nodata_loc_, f_texture.nodata());

        glUniform1i(dfdxy_image_loc_, 2);
        // glUniform1f(dfdxy_image_nodata_loc_, dfdxy_texture.nodata());

        glUniform1i(in_lvl_loc_, in_lvl);
        glUniform1i(out_lvl_loc_, out_lvl);

        glUniform1i(out_width_loc_, W);
        glUniform1i(out_height_loc_, H);

        glUniform1f(fx_loc_, cam.GetParams()(0));
        glUniform1f(fy_loc_, cam.GetParams()(1));

        mesh.draw();

        restore_state();
    }

private:
    GLint view_matrix_loc_ = -1;
    GLint pose_matrix_loc_ = -1;

    GLint fx_loc_ = -1;
    GLint fy_loc_ = -1;

    GLint kf_image_loc_ = -1;
    GLint kf_image_nodata_loc_ = -1;

    GLint f_image_loc_ = -1;
    GLint f_image_nodata_loc_ = -1;

    GLint dfdxy_image_loc_ = -1;
    GLint dfdxy_image_nodata_loc_ = -1;

    GLint in_lvl_loc_ = -1;
    GLint out_lvl_loc_ = -1;

    GLint out_width_loc_ = -1;
    GLint out_height_loc_ = -1;
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

            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec3 v_f_ver;
            out vec3 v_kf_ray;
            out vec2 v_texcoord;
            flat out int v_vertexID;

            void main() {
                vec4 ver = pose_matrix * vec4(a_position, 1.0);
                vec3 rray = mat3(pose_matrix) * a_position/a_position.z;
                gl_Position = view_matrix * ver;
                v_f_ver = ver.xyz;
                v_kf_ray = rray.xyz;
                v_texcoord = a_texcoord;
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
            flat in int v_vertexID[];           // from VS (Option A)

            out vec3 f_ver;
            out vec3 kf_ray;
            out vec2 texcoord;
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
            layout(location = 1) out vec3 pids_output;
            layout(location = 2) out float r_output;

            in vec3 f_ver;
            in vec3 kf_ray;
            in vec2 texcoord;

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

            void main()
            {
                // ivec2 tex_size = textureSize(f_image, f_image_lvl);

                float kf = textureLod(kf_image, texcoord, float(in_lvl)).r;
                float f = texelFetch(f_image, ivec2(gl_FragCoord.x, gl_FragCoord.y), out_lvl).r;
                vec2 dfdxy = texelFetch(dfdxy_image, ivec2(gl_FragCoord.x, gl_FragCoord.y), out_lvl).xy;

                //if (kf == kf_image_nodata || f == f_image_nodata || dfdxy.xy == dfdxy_image_nodata.xy)
                //{
                //    discard;
                //}

                float r = f - kf;

                float v0 = dfdxy.x * fx * out_width / f_ver.z;
                float v1 = dfdxy.y * fy * out_height / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                vec3 d_f_i_d_f_ver = vec3(v0, v1, v2);
                //vec3f d_f_i_d_rot = vec3(-f_ver.z * v1 + f_ver.y * v2, f_ver.z * v0 - f_ver.x * v2, -f_ver.y * v0 + f_ver.x * v1);

                //jtra_output = d_f_i_d_tra;
                //jrot_output = d_f_i_d_rot;

                vec3 d_f_ver_d_kf_depth = kf_ray;
                float d_f_i_d_kf_depth = dot(d_f_i_d_f_ver, d_f_ver_d_kf_depth);

                vec3 d_depth_d_vert_depth = bc;

                vec3 jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;

                jmap_output = jac;
                pids_output = triIDs;
                r_output = r;
            }
            )Shader";

        CompileShaders(vertex_shader, geometry_shader, fragment_shader);

        view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");

        fx_loc_ = glGetUniformLocation(program_, "fx");
        fy_loc_ = glGetUniformLocation(program_, "fy");

        kf_image_loc_ = glGetUniformLocation(program_, "kf_image");
        kf_image_nodata_loc_ = glGetUniformLocation(program_, "kf_image_nodata");

        f_image_loc_ = glGetUniformLocation(program_, "f_image");
        f_image_nodata_loc_ = glGetUniformLocation(program_, "f_image_nodata");

        dfdxy_image_loc_ = glGetUniformLocation(program_, "dfdxy_image");
        dfdxy_image_nodata_loc_ = glGetUniformLocation(program_, "dfdxy_image_nodata");

        in_lvl_loc_ = glGetUniformLocation(program_, "in_lvl");
        out_lvl_loc_ = glGetUniformLocation(program_, "out_lvl");

        out_width_loc_ = glGetUniformLocation(program_, "out_width");
        out_height_loc_ = glGetUniformLocation(program_, "out_height");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<ImageType> &kf_texture,
                const TextureGL<ImageType> &f_texture,
                const TextureGL<Vec3<float>> &dfdxy_texture,
                TextureGL<Vec3<float>> &jmap_texture,
                TextureGL<Vec3<PidType>> &pids_texture,
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

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(jmap_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(jmap_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        Vec3<float> jmap_nodata = jmap_texture.nodata();
        Vec3<float> pids_nodata = pids_texture.nodata();
        float r_nodata = r_texture.nodata();

        float jmap_clear[4] = {jmap_nodata(0), jmap_nodata(1), jmap_nodata(2), 1.f};
        float pids_clear[4] = {pids_nodata(0), pids_nodata(1), pids_nodata(2), 1.f};
        float r_clear[4] = {r_nodata, 0.f, 0.f, 1.f};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, jmap_clear);
        //         glClearBufferfv(GL_COLOR, 1, pids_clear);
        //         glClearBufferfv(GL_COLOR, 2, r_clear);
        // #else
        //  Clear GL_COLOR_ATTACHMENT0
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
        const GLenum bufs_restore[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
        glDrawBuffers(3, bufs_restore);
        // #endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, kf_texture.id());
            glBindTextureUnit(1, f_texture.id());
            glBindTextureUnit(2, dfdxy_texture.id());
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

        const Mat4<float> view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl_;
        const Mat4<float> pose_matrix = pose.matrix();

        glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, view_matrix.data());
        glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, pose_matrix.data());

        glUniform1i(kf_image_loc_, 0);
        glUniform1f(kf_image_nodata_loc_, kf_texture.nodata());

        glUniform1i(f_image_loc_, 1);
        glUniform1f(f_image_nodata_loc_, f_texture.nodata());

        glUniform1i(dfdxy_image_loc_, 2);
        // glUniform3f(dfdxy_image_nodata_loc_, dfdxy_texture.nodata());

        glUniform1i(in_lvl_loc_, in_lvl);
        glUniform1i(out_lvl_loc_, out_lvl);

        glUniform1i(out_width_loc_, W);
        glUniform1i(out_height_loc_, H);

        glUniform1f(fx_loc_, cam.GetParams()(0));
        glUniform1f(fy_loc_, cam.GetParams()(1));

        mesh.draw();

        restore_state();
    }

private:
    GLint view_matrix_loc_ = -1;
    GLint pose_matrix_loc_ = -1;

    GLint fx_loc_ = -1;
    GLint fy_loc_ = -1;

    GLint kf_image_loc_ = -1;
    GLint kf_image_nodata_loc_ = -1;

    GLint f_image_loc_ = -1;
    GLint f_image_nodata_loc_ = -1;

    GLint dfdxy_image_loc_ = -1;
    GLint dfdxy_image_nodata_loc_ = -1;

    GLint in_lvl_loc_ = -1;
    GLint out_lvl_loc_ = -1;

    GLint out_width_loc_ = -1;
    GLint out_height_loc_ = -1;
};

class DiffRendererGL : public BaseRendererGL
{
public:
    DiffRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;

            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec3 v_f_ver;
            out vec3 v_kf_ray;
            out vec2 v_texcoord;
            flat out int v_vertexID;

            void main() {
                vec4 ver = pose_matrix * vec4(a_position, 1.0);
                vec3 kf_ray = a_position/a_position.z;
                //vec3 rray = mat3(pose_matrix) * a_position/a_position.z;
                vec3 rray = (pose_matrix * vec4(kf_ray, 0.0)).xyz;
                gl_Position = view_matrix * ver;
                v_f_ver = ver.xyz;
                v_kf_ray = rray.xyz;
                v_texcoord = a_texcoord;
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
            flat in int v_vertexID[];           // from VS (Option A)

            out vec3 f_ver;
            out vec3 kf_ray;
            out vec2 texcoord;
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
            layout(location = 0) out float image_output;
            layout(location = 1) out float depth_output;
            layout(location = 2) out vec3 jtra_output;
            layout(location = 3) out vec3 jrot_output;
            layout(location = 4) out vec3 jmap_output;
            layout(location = 5) out vec3 pids_output;

            in vec3 f_ver;
            in vec3 kf_ray;
            in vec2 texcoord;

            smooth in vec3  bc;          // or noperspective if chosen above
            flat   in ivec3 triIDs;

            uniform sampler2D f_image;
            uniform float f_image_nodata;
            uniform int f_image_lvl;

            uniform sampler2D didxy_image;
            //uniform float didxy_image_nodata;
            uniform int didxy_image_lvl;

            uniform float fx;
            uniform float fy;

            uniform int out_width;
            uniform int out_height;

            vec2 get_dfdxy(sampler2D image, ivec2 tc, ivec2 tex_size, float nodata, int lvl)
            {
                int x_p = tc.x + 1;
                int x_m = tc.x - 1;
                int y_p = tc.y + 1;
                int y_m = tc.y - 1;

                if (x_p >= tex_size.x || x_m < 0 || y_p >= tex_size.y || y_m < 0)
                {
                    return vec2(nodata, nodata);
                }

                //Scalar f = Scalar(tex.texel_(y, x, lvl));
                float f_y_p = texelFetch(image, ivec2(tc.x, y_p), lvl).r;
                float f_y_m = texelFetch(image, ivec2(tc.x, y_m), lvl).r;
                float f_x_p = texelFetch(image, ivec2(x_p, tc.y), lvl).r;
                float f_x_m = texelFetch(image, ivec2(x_m, tc.y), lvl).r;

                //if (f_x_p == nodata || f_x_m == nodata || f_y_p == nodata || f_y_m == nodata)
                //{
                //    return vec2(nodata, nodata);
                //}

                return vec2((f_x_p - f_x_m) / 2.0f, (f_y_p - f_y_m) / 2.0f);
            }

            void main()
            {
                ivec2 tex_size = textureSize(f_image, f_image_lvl);

                //float f = texelFetch(f_image, ivec2(gl_FragCoord.xy), f_image_lvl).r;
                float f = textureLod(f_image, texcoord, float(f_image_lvl)).r;

                //if (f == f_image_nodata)
                //{
                //    discard;
                //}

                //vec2 dfdxy = get_dfdxy(f_image, ivec2(texcoord.x*tex_size.x, texcoord.y*tex_size.y), tex_size, f_image_nodata, f_image_lvl);
                vec2 dfdxy = textureLod(didxy_image, texcoord, float(didxy_image_lvl)).xy;

                //if(dfdxy.x == f_image_nodata && dfdxy.y == f_image_nodata)
                //{
                //    discard;
                //}

                float v0 = dfdxy.x * fx * out_width / f_ver.z;
                float v1 = dfdxy.y * fy * out_height / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                vec3 d_f_i_d_f_ver = vec3(v0, v1, v2);
                vec3 d_f_i_d_rot = vec3(-f_ver.z * v1 + f_ver.y * v2, f_ver.z * v0 - f_ver.x * v2, -f_ver.y * v0 + f_ver.x * v1);

                //jtra_output = d_f_i_d_tra;
                //jrot_output = d_f_i_d_rot;

                vec3 d_f_ver_d_kf_depth = kf_ray;
                float d_f_i_d_kf_depth = dot(d_f_i_d_f_ver, d_f_ver_d_kf_depth);

                vec3 d_depth_d_vert_depth = bc;

                vec3 jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;

                image_output = f;
                depth_output = f_ver.z;
                jtra_output = d_f_i_d_f_ver;
                jrot_output = d_f_i_d_rot;
                jmap_output = jac;
                pids_output = triIDs;
            }
            )Shader";

        CompileShaders(vertex_shader, geometry_shader, fragment_shader);

        view_matrix_loc_ = glGetUniformLocation(program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(program_, "pose_matrix");

        fx_loc_ = glGetUniformLocation(program_, "fx");
        fy_loc_ = glGetUniformLocation(program_, "fy");

        f_image_loc_ = glGetUniformLocation(program_, "f_image");
        f_image_nodata_loc_ = glGetUniformLocation(program_, "f_image_nodata");
        f_image_lvl_loc_ = glGetUniformLocation(program_, "f_image_lvl");

        didxy_image_loc_ = glGetUniformLocation(program_, "didxy_image");
        // didxy_image_nodata_loc_ = glGetUniformLocation(program_, "f_image_nodata");
        didxy_image_lvl_loc_ = glGetUniformLocation(program_, "didxy_image_lvl");

        out_width_loc_ = glGetUniformLocation(program_, "out_width");
        out_height_loc_ = glGetUniformLocation(program_, "out_height");
    }

    void Render(const MeshGL &mesh,
                const SE3<float> &pose,
                const PinholeCamera<float> &cam,
                int in_lvl,
                int out_lvl,
                const TextureGL<ImageType> &diffuse_texture,
                const TextureGL<Vec3<float>> &didxy_texture,
                TextureGL<ImageType> &image_texture,
                TextureGL<float> &depth_texture,
                TextureGL<Vec3<float>> &jtra_texture,
                TextureGL<Vec3<float>> &jrot_texture,
                TextureGL<Vec3<float>> &jmap_texture,
                TextureGL<Vec3<PidType>> &pids_texture)
    {
        save_state();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, image_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, depth_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, jtra_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, jrot_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, jmap_texture.id(), out_lvl);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, pids_texture.id(), out_lvl);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_); // now actually attach it

        const GLenum bufs[6] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5};
        glDrawBuffers(6, bufs);

        check_framebuffer();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        // glEnable(GL_SCISSOR_TEST);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); // was GL_CCW

        const GLsizei W = static_cast<GLsizei>(jmap_texture.width(out_lvl));
        const GLsizei H = static_cast<GLsizei>(jmap_texture.height(out_lvl));
        glViewport(0, 0, W, H);

        float image_nodata = image_texture.nodata();
        float depth_nodata = depth_texture.nodata();
        Vec3<float> jtra_nodata = jtra_texture.nodata();
        Vec3<float> jrot_nodata = jrot_texture.nodata();
        Vec3<float> jmap_nodata = jmap_texture.nodata();
        Vec3<PidType> pids_nodata = pids_texture.nodata();

        float image_clear[4] = {image_nodata, 0, 0, 1.f};
        float depth_clear[4] = {depth_nodata, 0, 0, 1.f};
        float jtra_clear[4] = {jtra_nodata(0), jtra_nodata(1), jtra_nodata(2), 1.f};
        float jrot_clear[4] = {jrot_nodata(0), jrot_nodata(1), jrot_nodata(2), 1.f};
        float jmap_clear[4] = {jmap_nodata(0), jmap_nodata(1), jmap_nodata(2), 1.f};
        PidType pids_clear[4] = {pids_nodata(0), pids_nodata(1), pids_nodata(2), 1};

        // #if defined(GL_VERSION_3_0)
        //         glClearBufferfv(GL_COLOR, 0, jmap_clear);
        //         glClearBufferfv(GL_COLOR, 1, pids_clear);
        //         glClearBufferfv(GL_COLOR, 2, r_clear);
        // #else
        //  Clear GL_COLOR_ATTACHMENT0
        const GLenum bufs0[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs0);
        glClearColor(image_clear[0], image_clear[1], image_clear[2], image_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        //  Clear GL_COLOR_ATTACHMENT0
        const GLenum bufs1[1] = {GL_COLOR_ATTACHMENT1};
        glDrawBuffers(1, bufs1);
        glClearColor(depth_clear[0], depth_clear[1], depth_clear[2], depth_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        //  Clear GL_COLOR_ATTACHMENT0
        const GLenum bufs2[1] = {GL_COLOR_ATTACHMENT2};
        glDrawBuffers(1, bufs2);
        glClearColor(jtra_clear[0], jtra_clear[1], jtra_clear[2], jtra_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        //  Clear GL_COLOR_ATTACHMENT0
        const GLenum bufs3[1] = {GL_COLOR_ATTACHMENT3};
        glDrawBuffers(1, bufs3);
        glClearColor(jrot_clear[0], jrot_clear[1], jrot_clear[2], jrot_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT2
        const GLenum bufs4[1] = {GL_COLOR_ATTACHMENT4};
        glDrawBuffers(1, bufs4);
        glClearColor(jmap_clear[0], jmap_clear[1], jmap_clear[2], jmap_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        // Clear GL_COLOR_ATTACHMENT1
        const GLenum bufs5[1] = {GL_COLOR_ATTACHMENT5};
        glDrawBuffers(1, bufs5);
        glClearColor(pids_clear[0], pids_clear[1], pids_clear[2], pids_clear[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Restore glDrawBuffers for subsequent rendering.
        // This assumes the original setup was GL_COLOR_ATTACHMENT0 and GL_COLOR_ATTACHMENT1
        // as done in the clear_buffers function.
        const GLenum bufs_restore[6] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5};
        glDrawBuffers(6, bufs_restore);
        // #endif

#if defined(GL_VERSION_4_5)
        if (GLAD_GL_VERSION_4_5)
        {
            glBindTextureUnit(0, diffuse_texture.id());
            glBindTextureUnit(1, didxy_texture.id());
        }
        else
#endif
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, diffuse_texture.id());
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, didxy_texture.id());
        }

        glUseProgram(program_);

        const Mat4<float> view_matrix = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * opencv2opengl_;
        const Mat4<float> pose_matrix = pose.matrix();

        glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, view_matrix.data());
        glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, pose_matrix.data());

        glUniform1i(f_image_loc_, 0);
        glUniform1f(f_image_nodata_loc_, diffuse_texture.nodata());
        glUniform1i(f_image_lvl_loc_, in_lvl);

        glUniform1i(didxy_image_loc_, 1);
        // glUniform1f(didxy_image_nodata_loc_, diffuse_texture.nodata());
        glUniform1i(didxy_image_lvl_loc_, in_lvl);

        glUniform1f(fx_loc_, cam.GetParams()(0));
        glUniform1f(fy_loc_, cam.GetParams()(1));

        glUniform1i(out_width_loc_, W);
        glUniform1i(out_height_loc_, H);

        mesh.draw();

        restore_state();
    }

private:
    GLint view_matrix_loc_ = -1;
    GLint pose_matrix_loc_ = -1;

    GLint fx_loc_ = -1;
    GLint fy_loc_ = -1;

    GLint f_image_loc_ = -1;
    GLint f_image_nodata_loc_ = -1;
    GLint f_image_lvl_loc_ = -1;

    GLint didxy_image_loc_ = -1;
    GLint didxy_image_nodata_loc_ = -1;
    GLint didxy_image_lvl_loc_ = -1;

    GLint out_width_loc_ = -1;
    GLint out_height_loc_ = -1;
};