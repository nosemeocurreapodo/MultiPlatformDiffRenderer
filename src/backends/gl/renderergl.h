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

        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &pos_bo_);
        glGenBuffers(1, &tex_bo_);
        glGenBuffers(1, &wei_bo_);
        glGenBuffers(1, &ebo_);

        glGenTextures(1, &in_tex_);
        glBindTexture(GL_TEXTURE_2D, in_tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenTextures(1, &out_tex_);
        glBindTexture(GL_TEXTURE_2D, out_tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // GLint internal_format = GetGLInternalFormat(GetTypeIndex<OutTexType>(), out_channels);
        // GLenum format = GetGLFormat(texture.channels());
        // GLenum type = GetGLType(GetTypeIndex<InTexType>());
        // glTexImage2D(GL_TEXTURE_2D, 0, internal_format_, width_, height_, 0, format_, type_, nullptr);
    }

    void WriteMesh(const MeshCPU &mesh)
    {
        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, pos_bo_);
        glBufferData(GL_ARRAY_BUFFER, mesh.GetPosBuffer().size() * sizeof(float), mesh.GetPosBuffer().get(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, tex_bo_);
        glBufferData(GL_ARRAY_BUFFER, mesh.GetTexBuffer().size() * sizeof(float), mesh.GetTexBuffer().get(), GL_STATIC_DRAW);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, wei_bo_);
        glBufferData(GL_ARRAY_BUFFER, mesh.GetWeiBuffer().size() * sizeof(float), mesh.GetWeiBuffer().get(), GL_STATIC_DRAW);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, (void *)0);
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.GetEboBuffer().size() * sizeof(unsigned int), mesh.GetEboBuffer().get(), GL_STATIC_DRAW);
        ebo_size_ = mesh.GetEboBuffer().size();

        glBindVertexArray(0);
    }

    void WriteInTexture(const TextureCPU<InTexType> &texture)
    {
        GLint internal_format = GetGLInternalFormat(GetTypeIndex<InTexType>(), texture.channels());
        GLenum format = GetGLFormat(texture.channels());
        GLenum type = GetGLType(GetTypeIndex<InTexType>());
        glBindTexture(GL_TEXTURE_2D, in_tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, texture.width(), texture.height(), 0, format, type, texture.get());

        // glBindTexture(GL_TEXTURE_2D, texture_id_);
        // glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, format_, type_, data);
        // glGenerateMipmap(GL_TEXTURE_2D);
    }

    void PrepareOutTexture(const TextureCPU<OutTexType> &texture)
    {
        GLint internal_format = GetGLInternalFormat(GetTypeIndex<OutTexType>(), texture.channels());
        GLenum format = GetGLFormat(texture.channels());
        GLenum type = GetGLType(GetTypeIndex<OutTexType>());
        out_tex_width_ = texture.width();
        out_tex_height_ = texture.height();
        glBindTexture(GL_TEXTURE_2D, out_tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, texture.width(), texture.height(), 0, format, type, texture.get());
    }

    void ReadOutTexture(TextureCPU<OutTexType> &texture) const
    {
        GLint internal_format = GetGLInternalFormat(GetTypeIndex<OutTexType>(), texture.channels());
        GLenum format = GetGLFormat(texture.channels());
        GLenum type = GetGLType(GetTypeIndex<OutTexType>());
        glBindTexture(GL_TEXTURE_2D, out_tex_);
        glGetTexImage(GL_TEXTURE_2D, 0, format, type, texture.get());

        // glGetBufferSubData(buffer_type, 0, size_ * sizeof(Type), data);
        // glBufferSubData(buffer_type, 0, size_ * sizeof(Type), data);
    }

    void Render(const cpu::SE3 pose, const cpu::Camera cam, int lvl)
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
                               out_tex_,
                               lvl);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "FBO incomplete, status = 0x" << std::hex << status << std::dec << "\n";
            return;
        }

        glDisable(GL_CULL_FACE);
        glViewport(0, 0, out_tex_width_, out_tex_height_);
        glClearColor(0.3f, 0.3f, 0.3f, 1.0f); // dark-blue background
        glClear(GL_COLOR_BUFFER_BIT);

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

        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(ebo_size_), GL_UNSIGNED_INT, 0);

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

    GLuint vao_;
    GLuint pos_bo_;
    GLuint tex_bo_;
    GLuint wei_bo_;
    GLuint ebo_;

    int ebo_size_;

    GLuint in_tex_;
    //GLint in_tex_internal_format_;
    //GLenum in_tex_format_;
    //GLenum in_tex_type_;
    //InTexType in_tex_nodata_;
    //int in_tex_width_;
    //int in_tex_height_;
    //int in_tex_channels_;

    GLuint out_tex_;
    //GLint out_tex_internal_format_;
    //GLenum out_tex_format_;
    //GLenum out_tex_type_;
    //OutTexType out_tex_nodata_;
    int out_tex_width_;
    int out_tex_height_;
    int out_tex_channels_;
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