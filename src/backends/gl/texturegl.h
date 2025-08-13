#pragma once

#include "backends/gl/devicegl_glad.h"

template <typename Type>
class TextureGL
{
    template <typename InTexType, typename OutTexType>
    friend class BaseRendererGL;
    friend class ImageRendererGL;

public:
    TextureGL() : nodata_(0), width_(0), height_(0)
    {
        // data_ = nullptr;
    }

    TextureGL(int width, int height, Type nodata_value)
    {
        glGenTextures(1, &tex_);
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int channels = cpu::getChannels<Type>();

        GLint internal_format = GetGLInternalFormat(GetTypeIndex<Type>());
        GLenum format = GetGLFormat(channels);
        GLenum type = GetGLType(GetTypeIndex<Type>());
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, nullptr);

        nodata_ = nodata_value;
        width_ = width;
        height_ = height;

        // data_.fill(nodata_value);
    }

    TextureGL(int width, int height, Type nodata_value, Type *data)
    {
        glGenTextures(1, &tex_);
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int channels = cpu::getChannels<Type>();

        GLint internal_format = GetGLInternalFormat(GetTypeIndex<Type>());
        GLenum format = GetGLFormat(channels);
        GLenum type = GetGLType(GetTypeIndex<Type>());
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, data);

        nodata_ = nodata_value;
        width_ = width;
        height_ = height;
    }

    /*
    TextureGPU(const TextureGPU &other)
        : data_(other.data_)
    {
        nodata_ = other.nodata_;
        width_ = other.width_;
        height_ = other.height_;
        channels_ = other.channels_;
    }

    TextureGPU &operator=(const TextureGPU &other)
    {
        if (this != &other)
        {
            nodata_ = other.nodata_;
            width_ = other.width_;
            height_ = other.height_;
            channels_ = other.channels_;
            data_ = other.data_;
        }
        return *this;
    }
    */

    void FromCPU(const Type *data)
    {
        int channels = cpu::getChannels<Type>();

        GLint internal_format = GetGLInternalFormat(GetTypeIndex<Type>());
        GLenum format = GetGLFormat(channels);
        GLenum type = GetGLType(GetTypeIndex<Type>());

        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_), 0, format, type, data);
    }

    void ToCPU(Type *data) const
    {
        // data_.ToCPU(data);

        // GLint internal_format = GetGLInternalFormat(GetTypeIndex<Type>(), channels);

        int channels = cpu::getChannels<Type>();
        GLenum format = GetGLFormat(channels);
        GLenum type = GetGLType(GetTypeIndex<Type>());
        glBindTexture(GL_TEXTURE_2D, tex_);
        glGetTexImage(GL_TEXTURE_2D, 0, format, type, data);
    }

    unsigned int width() const
    {
        return width_;
    }
    unsigned int height() const
    {
        return height_;
    }
    unsigned int size() const
    {
        return width_ * height_;
    }

    Type nodata() const
    {
        return nodata_;
    }

protected:
    GLuint tex_;
    unsigned int width_;
    unsigned int height_;
    Type nodata_;
};
