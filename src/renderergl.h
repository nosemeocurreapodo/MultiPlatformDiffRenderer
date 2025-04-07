#pragma once

#include "common/devicegl.h"
#include "common/texturegl.h"
#include "common/buffergl.h"
#include "common/meshgl.h"

class BaseRendererGL
{
public:
    BaseRendererGL()
    {
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
    GLint mvp_loc_;

    const char *vertex_shader_;
    const char *fragment_shader_;
};

class DepthRendererGL : public BaseRendererGL
{
public:
    DepthRendererGL()
    {
        vertex_shader_ = R"Shader(
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

        fragment_shader_ = R"Shader(
            #version 330 core
            layout(location = 0) out float f_color;
            in float depth;
            
            void main() {
                f_color = depth;
            }
            )Shader";

        CompileShaders(vertex_shader_, fragment_shader_);
        mvp_loc_ = glGetUniformLocation(shader_program_, "MVP");
    }

    void Render(MeshGL &mesh, SE3 pose, CameraType cam, TextureGL<float> &buffer, int lvl)
    {
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, buffer.texture_id_, lvl);

        Mat4 view_matrix = cam.GetProjectiveMatrix(0.0f, 10.0f) * pose.matrix();

        glUseProgram(shader_program_);
        GLfloat mvp_float[16];
        for (int i = 0; i < 16; ++i)
        {
            mvp_float[i] = static_cast<GLfloat>(view_matrix.data()[i]);
        }
        glUniformMatrix4fv(mvp_loc_, 1, GL_FALSE, mvp_float);

        glBindVertexArray(mesh.vao_);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.tri_size_ * 3), GL_UNSIGNED_INT, 0);
        // glDrawElements(GL_LINE_STRIP, static_cast<GLsizei>(indicesSize), GL_UNSIGNED_INT, 0);
        // glDrawArrays(GL_LINE_STRIP, 0, verticesSize);
        glBindVertexArray(0);
        glUseProgram(0);
    }

private:
};