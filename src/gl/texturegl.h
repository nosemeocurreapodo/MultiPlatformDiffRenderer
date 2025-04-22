#pragma once

#include "gl/devicegl_glad.h"
#include "texture.h"

template <typename Type>
class TextureGL : public Texture<Type>
{
    template <typename InTexType, typename OutTexType>
    friend class BaseRendererGL;
    friend class DepthRendererGL;
    friend class ImageRendererGL;

public:
    /*
    TextureGL() : nodata_(0), width_(0), height_(0), channels_(0)
    {
        glGenTextures(1, &texture_id_);
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    */
    TextureGL(int width, int height, int channels, Type nodata_value)
    {
        if (channels > 4)
            throw std::runtime_error("Too many channels in dataGL");

        nodata_ = nodata_value;
        width_ = width;
        height_ = height;
        channels_ = channels;

        internal_format_ = GetGLInternalFormat(GetTypeIndex<Type>(), channels);
        format_ = GetGLFormat(channels);
        type_ = GetGLType(GetTypeIndex<Type>());

        glGenTextures(1, &texture_id_);
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format_, width_, height_, 0, format_, type_, nullptr);
        // glGenerateMipmap(GL_TEXTURE_2D);
    }

    TextureGL(const TextureGL &other)
    {
        nodata_ = other.nodata_;
        width_ = other.width_;
        height_ = other.height_;
        channels_ = other.channels_;
        /*
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);

        // Attach the source texture to the FBO.
        glFramebufferTexture2D(
            GL_READ_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            other.texture_id_, // <-- your source texture handle
            0                  // mipmap level
        );
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        glGenTextures(1, &texture_id_);
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glCopyTexSubImage2D(
            GL_TEXTURE_2D,
            0,              // mipmap level
            0, 0,           // xoffset, yoffset in dest texture
            0, 0,           // x, y in the read buffer to start copying from
            width_, height_ // width, height to copy
        );
        */
    }

    TextureGL &operator=(const TextureGL &other)
    {
        if (this != &other)
        {

            nodata_ = other.nodata_;
            width_ = other.width_;
            height_ = other.height_;
            channels_ = other.channels_;

            /*
            glDeleteTextures(1, &texture_id_);

            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);

            // Attach the source texture to the FBO.
            glFramebufferTexture2D(
                GL_READ_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D,
                other.texture_id_, // <-- your source texture handle
                0                  // mipmap level
            );
            glReadBuffer(GL_COLOR_ATTACHMENT0);

            glGenTextures(1, &texture_id_);
            glBindTexture(GL_TEXTURE_2D, texture_id_);
            glCopyTexSubImage2D(
                GL_TEXTURE_2D,  // target
                0,              // mipmap level
                0, 0,           // xoffset, yoffset in dest texture
                0, 0,           // x, y in the read buffer to start copying from
                width_, height_ // width, height to copy
            );
            */
        }
        return *this;
    }

    void FromCPU(Type *data)
    {
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, format_, type_, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    void ToCPU(Type *data)
    {
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glGetTexImage(GL_TEXTURE_2D, 0, format_, type_, data);
    }

    /*
    ~data()
    {
        delete[] m_data;
        // m_data = nullptr;
    }
    */

    Type nodata_;
    int width_;
    int height_;
    int channels_;

private:
    GLuint texture_id_;
    GLint internal_format_;
    GLenum format_;
    GLenum type_;
};
