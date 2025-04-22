#pragma once

#include "renderer.h"
#include "gl/devicegl_glad.h"
#include "gl/texturegl.h"
#include "gl/buffergl.h"
#include "gl/meshgl.h"

template <typename InTexType, typename OutTexType>
class BaseRendererGL : public Renderer<InTexType, OutTexType>
{
public:
    BaseRendererGL()
    {
        glGenFramebuffers(1, &fbo);
    }

    void Render(const MeshGL &mesh, const SE3 pose, const CameraType cam, const TextureGL<InTexType> &texture, TextureGL<OutTexType> &buffer, int lvl)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        // glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, buffer.texture_id_, lvl);

        // unsigned int drawbuffers[] = {GL_COLOR_ATTACHMENT0};
        // glDrawBuffers(sizeof(drawbuffers) / sizeof(unsigned int), drawbuffers);
        // if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        //     std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete! calcResidual" << std::endl;

        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D,
                               buffer.texture_id_,
                               lvl);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "FBO incomplete, status = 0x" << std::hex << status << std::dec << "\n";
            return;
        }

        glDisable(GL_CULL_FACE);
        glViewport(0, 0, buffer.width_, buffer.height_);
        glClearColor(0.3f, 0.3f, 0.3f, 1.0f); // dark-blue background
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shader_program_);

        Mat4 opencv2opengl = Mat4::Identity();
        opencv2opengl(1, 1) = 1.0;
        opencv2opengl(2, 2) = -1.0;

        Mat4 view_matrix = cam.GetProjectiveMatrix(0.01f, 100.0f) * opencv2opengl * pose.matrix();

        glUseProgram(shader_program_);
        GLfloat mvp_float[16];
        for(int y = 0; y < 4; ++y)
        {
            for(int x = 0; x < 4; ++x)
            {
                mvp_float[x * 4 + y] = static_cast<GLfloat>(view_matrix(y, x));
            }
        }
        glUniformMatrix4fv(mvp_loc_, 1, GL_FALSE, mvp_float);

        glBindVertexArray(mesh.vao_);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.tri_size_), GL_UNSIGNED_INT, 0);

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
    GLuint fbo;
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