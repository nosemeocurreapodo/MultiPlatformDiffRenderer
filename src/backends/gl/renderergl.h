#pragma once

#include "backends/gl/devicegl_glad.h"
#include "core/format_converters.h"
#include "core/camera.h"

template <typename InTexType, typename OutTexType>
class BaseRendererGL
{
public:
    BaseRendererGL()
    {
        glGenFramebuffers(1, &fbo_);

        // GLint internal_format = GetGLInternalFormat(GetTypeIndex<OutTexType>(), out_channels);
        // GLenum format = GetGLFormat(texture.channels());
        // GLenum type = GetGLType(GetTypeIndex<InTexType>());
        // glTexImage2D(GL_TEXTURE_2D, 0, internal_format_, width_, height_, 0, format_, type_, nullptr);
    }

    void Render(const MeshGL &mesh,
                const cpu::SE3 pose,
                const cpu::Camera cam,
                const TextureGL<InTexType> &texture_in,
                TextureGL<OutTexType> &texture_out,
                int in_lvl,
                int out_lvl)
    {
        pose_ = pose;
        cam_ = cam;
        in_nodata_ = texture_in.nodata();
        out_nodata_ = texture_out.nodata();
        int in_channels = cpu::getChannels<InTexType>();
        int out_channels = cpu::getChannels<OutTexType>();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        // glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, buffer.texture_id_, lvl);

        // unsigned int drawbuffers[] = {GL_COLOR_ATTACHMENT0};
        // glDrawBuffers(sizeof(drawbuffers) / sizeof(unsigned int), drawbuffers);
        // if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        //     std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete! calcResidual" << std::endl;

        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D,
                               texture_out.tex_,
                               out_lvl);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "FBO incomplete, status = 0x" << std::hex << status << std::dec << "\n";
            return;
        }

        glDisable(GL_CULL_FACE);
        glViewport(0, 0, texture_out.width_, texture_out.height_);

        /*
        if (out_channels == 1)
            glClearColor(out_nodata_, 0.0f, 0.0f, 1.0f);
        else if (out_channels == 2)
            glClearColor(out_nodata_(0), out_nodata_(1), 0.0f, 1.0f);
        else if (out_channels == 3)
            glClearColor(out_nodata_(0), out_nodata_(1), out_nodata_(2), 1.0f);
        else if (out_channels == 4)
            glClearColor(out_nodata_(0), out_nodata_(1), out_nodata_(2), out_nodata_(3));
        else
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        */

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // int channels = cpu::getChannels<InTexType>();

        glActiveTexture(GL_TEXTURE0);
        glBindImageTexture(0, texture_in.tex_, in_lvl, GL_FALSE, 0, GL_READ_ONLY, GetGLInternalFormat(GetTypeIndex<InTexType>()));

        glUseProgram(shader_program_);

        cpu::Mat4 opencv2opengl = cpu::Mat4::Identity();
        opencv2opengl(1, 1) = 1.0;
        opencv2opengl(2, 2) = -1.0;

        cpu::Mat4 view_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl;
        cpu::Mat4 pose_matrix = pose.matrix();

        glUseProgram(shader_program_);
        GLfloat view_matrix_float[16];
        GLfloat pose_matrix_float[16];
        for (int y = 0; y < 4; ++y)
        {
            for (int x = 0; x < 4; ++x)
            {
                view_matrix_float[x * 4 + y] = static_cast<GLfloat>(view_matrix(y, x));
                pose_matrix_float[x * 4 + y] = static_cast<GLfloat>(pose_matrix(y, x));
            }
        }
        glUniformMatrix4fv(view_matrix_loc_, 1, GL_FALSE, view_matrix_float);
        glUniformMatrix4fv(pose_matrix_loc_, 1, GL_FALSE, pose_matrix_float);

        glUniform1f(fx_loc_, cam.GetParams()(0));
        glUniform1f(fy_loc_, cam.GetParams()(1));

        glUniform1i(texture_loc, 0);

        /*
        if (in_channels == 1)
            glUniform1f(image_nodata_loc, static_cast<GLfloat>(in_nodata_(0)));
        else if (in_channels == 2)
            glUniform2f(image_nodata_loc,
                        static_cast<GLfloat>(in_nodata_(0)),
                        static_cast<GLfloat>(in_nodata_(1)));
        else if (in_channels == 3)
            glUniform3f(image_nodata_loc,
                        static_cast<GLfloat>(in_nodata_(0)),
                        static_cast<GLfloat>(in_nodata_(1)),
                        static_cast<GLfloat>(in_nodata_(2)));
        else if (in_channels == 4)
            glUniform4f(image_nodata_loc,
                        static_cast<GLfloat>(in_nodata_(0)),
                        static_cast<GLfloat>(in_nodata_(1)),
                        static_cast<GLfloat>(in_nodata_(2)),
                        static_cast<GLfloat>(in_nodata_(3)));
        */

        glBindVertexArray(mesh.vao_);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.ebo_size_), GL_UNSIGNED_INT, 0);

        glBindVertexArray(0);
        glUseProgram(0);
    }

protected:
    void CompileShaders(const char *vertex_shader, const char *fragment_shader)
    {
        // Build and compile our shader program
        // ------------------------------------

        // Create a vertex shader GL object
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);

        // Tell GL the source code to use
        glShaderSource(vertexShader, 1, &vertex_shader, NULL);

        // Actually compile the program
        glCompileShader(vertexShader);

        // Check if the compilation was successfull, and print anyn errors
        int success;
        char infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                      << infoLog << std::endl;
        }

        // Repeat for the Fragment shader
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragment_shader, NULL);
        glCompileShader(fragmentShader);
        // check for shader compile errors
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
                      << infoLog << std::endl;
        }

        // Link the vertex and fragment shaders into one complete program
        shader_program_ = glCreateProgram();

        glAttachShader(shader_program_, vertexShader);
        glAttachShader(shader_program_, fragmentShader);
        glLinkProgram(shader_program_);

        // Check for linking errors
        glGetProgramiv(shader_program_, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader_program_, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                      << infoLog << std::endl;
        }

        // Delete the now unused shader objects
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    unsigned int shader_program_;
    GLuint fbo_;
    GLint view_matrix_loc_;
    GLint pose_matrix_loc_;
    GLint fx_loc_;
    GLint fy_loc_;
    GLuint texture_loc;
    GLuint texture_nodata_loc;

    cpu::SE3 pose_;
    cpu::Camera cam_;
    InTexType in_nodata_;
    OutTexType out_nodata_;
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
            layout (location = 2) in vec3 a_weight;
            
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
            layout(location = 0) out float output;
            in float depth;

            uniform sampler2D texture;
            uniform float texture_nodata;

            void main()
            {
                output = depth;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
        view_matrix_loc_ = glGetUniformLocation(shader_program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(shader_program_, "pose_matrix");
        texture_loc = glGetUniformLocation(shader_program_, "texture");
        texture_nodata_loc = glGetUniformLocation(shader_program_, "texture_nodata");
    }

private:
};

class ImageRendererGL : public BaseRendererGL<cpu::ImageType /*InTexType*/, cpu::ImageType /*OutTexType*/>
{
public:
    ImageRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in vec3 a_weight;
            
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
            layout(location = 0) out float f_color;
            in vec2 texcoord;

            uniform sampler2D image;
            uniform float image_nodata;

            void main()
            {
                f_color = texture(image, texcoord).r;
                //f_color = 100.0;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
        view_matrix_loc_ = glGetUniformLocation(shader_program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(shader_program_, "pose_matrix");
        texture_loc = glGetUniformLocation(shader_program_, "texture");
        texture_nodata_loc = glGetUniformLocation(shader_program_, "texture_nodata");
    }

private:
};

class DIDxyRendererGL : public BaseRendererGL<cpu::ImageType /*InTexType*/, cpu::Vec3 /*OutTexType*/>
{
public:
    DIDxyRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in vec3 a_weight;
            
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
            layout(location = 0) out vec3 f_color;
            in vec2 texcoord;

            uniform sampler2D image;
            uniform float image_nodata;

            void main()
            {
                ivec2 tex_size = textureSize(image, 0);
                int x = int(texcoord.x * (tex_size.x - 1));
                int y = int(texcoord.y * (tex_size.y - 1));
                int x_p = x + 1;
                int x_m = x - 1;
                int y_p = y + 1;
                int y_m = y - 1;

                if (x_p >= tex_size.x || x_m < 0 || y_p >= tex_size.y || y_m < 0)
                {
                    //no need to explicitly set to nodata, it is already in the background color
                    //f_color = nodata;
                    return;
                }

                float f_x_p = texelFetch(image, ivec2(x_p, y), 0).r;
                float f_x_m = texelFetch(image, ivec2(x_m, y), 0).r;
                float f_y_p = texelFetch(image, ivec2(x, y_p), 0).r;
                float f_y_m = texelFetch(image, ivec2(x, y_m), 0).r;

                if (f_x_p == image_nodata || f_x_m == image_nodata ||
                    f_y_p == image_nodata || f_y_m == image_nodata)
                {
                    //no need to explicitly set to nodata, it is already in the background color
                    //f_color = nodata;
                    return;
                }

                f_color.x = (f_x_p - f_x_m) / 2.0f;
                f_color.y = (f_y_p - f_y_m) / 2.0f;
                f_color.z = 0.0f;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
        view_matrix_loc_ = glGetUniformLocation(shader_program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(shader_program_, "pose_matrix");
        texture_loc = glGetUniformLocation(shader_program_, "texture");
        texture_nodata_loc = glGetUniformLocation(shader_program_, "texture_nodata");
    }

private:
};

class JtraRendererGL : public BaseRendererGL<cpu::Vec2 /*InTexType*/, cpu::Vec3 /*OutTexType*/>
{
public:
    JtraRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in vec3 a_weight;
            
            uniform mat4 view_matrix;
            uniform mat4 pose_matrix;

            out vec2 texcoord;
            out vec3 f_ver;
            flat out float fx;
            flat out float fy;

            void main() {
                f_ver = pose_matrix * vec4(a_position, 1.0);
                gl_Position = view_matrix * f_ver;

                fx = view_matrix(0, 0) / 2.0f;
                fy = view_matrix(1, 1) / 2.0f;
                
                // Pass texture coordinates to fragment shader
                texcoord = a_texcoord;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out float f_color;
            
            in vec2 texcoord;
            in vec3 f_ver;
            flat in float fx;
            flat in float fy;

            uniform sampler2D texture;
            uniform float texture_nodata;
            void main()
            {
                vec3 didxy = texture(texture, texcoord).xyz;
                //f_color = 100.0;

                float v0 = didxy.x * fx / f_ver.z;
                float v1 = didxy.y * fy / f_ver.z;
                float v2 = -(v0 * f_ver.x + v1 * f_ver.y) / f_ver.z;

                f_color = vec3f(v0, v1, v2);
                //vec3f d_f_i_d_rot(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
        view_matrix_loc_ = glGetUniformLocation(shader_program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(shader_program_, "pose_matrix");
        texture_loc = glGetUniformLocation(shader_program_, "texture");
        texture_nodata_loc = glGetUniformLocation(shader_program_, "texture_nodata");
    }

private:
};

class JPoseRendererGL : public BaseRendererGL<cpu::Vec2 /*InTexType*/, float /*OutTexType*/>
{
public:
    JPoseRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in vec3 a_weight;
            
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
            layout(location = 0) out float output;
            in vec2 texcoord;

            uniform sampler2D image;
            uniform float image_nodata;

            void main()
            {
                output = texture(image, texcoord).r;
                //f_color = 100.0;

                float v0 = f_der.x * fx * id;
                float v1 = f_der.y * fy * id;
                float v2 = -(v0 * pframe.x + v1 * pframe.y) * id;

                vec3f d_f_i_d_tra = vec3f(v0, v1, v2);
                vec3f d_f_i_d_rot(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
        view_matrix_loc_ = glGetUniformLocation(shader_program_, "view_matrix");
        pose_matrix_loc_ = glGetUniformLocation(shader_program_, "pose_matrix");
        texture_loc = glGetUniformLocation(shader_program_, "texture");
        texture_nodata_loc = glGetUniformLocation(shader_program_, "texture_nodata");
    }

private:
};