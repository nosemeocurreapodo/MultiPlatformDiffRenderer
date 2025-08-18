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
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);

        int channels = getChannels<Type>();

        GLint internal_format = GetGLInternalFormat(GetTypeIndex<Type>());
        GLenum format = GetGLFormat(channels);
        GLenum type = GetGLType(GetTypeIndex<Type>());
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, nullptr);
        glGenerateMipmap(GL_TEXTURE_2D);

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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);

        int channels = getChannels<Type>();

        GLint internal_format = GetGLInternalFormat(GetTypeIndex<Type>());
        GLenum format = GetGLFormat(channels);
        GLenum type = GetGLType(GetTypeIndex<Type>());
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, data);
        glGenerateMipmap(GL_TEXTURE_2D);

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

    void FromCPU(int lvl, const Type *data)
    {
        int channels = getChannels<Type>();

        GLint internal_format = GetGLInternalFormat(GetTypeIndex<Type>());
        GLenum format = GetGLFormat(channels);
        GLenum type = GetGLType(GetTypeIndex<Type>());

        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexImage2D(GL_TEXTURE_2D,
                     lvl,
                     internal_format,
                     static_cast<GLsizei>(width(lvl)),
                     static_cast<GLsizei>(height(lvl)),
                     0,
                     format,
                     type,
                     data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, lvl);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    void ToCPU(int lvl, Type *data) const
    {
        // data_.ToCPU(data);

        // GLint internal_format = GetGLInternalFormat(GetTypeIndex<Type>(), channels);

        int channels = getChannels<Type>();
        GLenum format = GetGLFormat(channels);
        GLenum type = GetGLType(GetTypeIndex<Type>());
        glBindTexture(GL_TEXTURE_2D, tex_);
        glGetTexImage(GL_TEXTURE_2D, lvl, format, type, data);
    }

    unsigned int width(int lvl) const
    {
        return int(width_ / std::pow(2, lvl));
    }
    unsigned int height(int lvl) const
    {
        return int(height_ / std::pow(2, lvl));
    }
    unsigned int size(int lvl) const
    {
        return int(width_ * height_ / std::pow(2, 2 * lvl));
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
