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
                int lvl)
    {
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
                               lvl);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "FBO incomplete, status = 0x" << std::hex << status << std::dec << "\n";
            return;
        }

        glDisable(GL_CULL_FACE);
        glViewport(0, 0, texture_out.width_, texture_out.height_);
        glClearColor(0.3f, 0.3f, 0.3f, 1.0f); // dark-blue background
        glClear(GL_COLOR_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindImageTexture(0, texture_in.tex_, lvl, GL_FALSE, 0, GL_READ_ONLY, GetGLInternalFormat(GetTypeIndex<InTexType>(), texture_in.channels_));

        glUseProgram(shader_program_);

        cpu::Mat4 opencv2opengl = cpu::Mat4::Identity();
        opencv2opengl(1, 1) = 1.0;
        opencv2opengl(2, 2) = -1.0;

        cpu::Mat4 view_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl * pose.matrix();

        glUseProgram(shader_program_);
        GLfloat mvp_float[16];
        for (int y = 0; y < 4; ++y)
        {
            for (int x = 0; x < 4; ++x)
            {
                mvp_float[x * 4 + y] = static_cast<GLfloat>(view_matrix(y, x));
            }
        }
        glUniformMatrix4fv(mvp_loc_, 1, GL_FALSE, mvp_float);

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
    GLint mvp_loc_;
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
            uniform mat4 MVP;

            void main() {
                gl_Position = MVP * vec4(a_position, 1.0);
                depth = a_position.z;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out float f_color;
            in float depth;

            void main()
            {
                f_color = depth;
                //f_color = 100.0;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
        mvp_loc_ = glGetUniformLocation(shader_program_, "MVP");
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
            uniform mat4 MVP;

            out vec2 texcoord;

            void main() {
                gl_Position = MVP * vec4(a_position, 1.0);
                texcoord = a_texcoord;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out float f_color;
            in vec2 texcoord;

            uniform sampler2D image;

            void main()
            {
                f_color = texture(image, texcoord).r;
                //f_color = 100.0;
            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
        mvp_loc_ = glGetUniformLocation(shader_program_, "MVP");
    }

private:
};

class JPoseRendererGL : public BaseRendererGL<cpu::ImageType /*InTexType*/, float /*OutTexType*/>
{
public:
    JPoseRendererGL() : BaseRendererGL()
    {
        const char *vertex_shader = R"Shader(
            #version 330 core
            layout (location = 0) in vec3 a_position;
            layout (location = 1) in vec2 a_texcoord;
            layout (location = 2) in vec3 a_weight;
            uniform mat4 MVP;

            out vec2 texcoord;

            void main() {
                gl_Position = MVP * vec4(a_position, 1.0);
                texcoord = a_texcoord;
            }
            )Shader";

        const char *fragment_shader = R"Shader(
            #version 330 core
            layout(location = 0) out float f_color;
            in vec2 texcoord;

            uniform sampler2D image;

            void main()
            {
                f_color = texture(image, texcoord).r;
                //f_color = 100.0;

                float v0 = f_der.x * fx * id;
                float v1 = f_der.y * fy * id;
                float v2 = -(v0 * pframe.x + v1 * pframe.y) * id;

                vec3f d_f_i_d_tra = vec3f(v0, v1, v2);
                vec3f d_f_i_d_rot(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

            }
            )Shader";

        CompileShaders(vertex_shader, fragment_shader);
        mvp_loc_ = glGetUniformLocation(shader_program_, "MVP");
    }

private:
};