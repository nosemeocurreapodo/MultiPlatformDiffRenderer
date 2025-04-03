#pragma once

#include <pangolin/pangolin.h>
#include "src/common/types.h"

template <typename Type>
class dataGL
{
public:

    dataGL(int _width, int _height, int _channels, Type _nodata_value)
    {
        nodata = _nodata_value;
        width = _width;
        height = _height;
        channels = _channels;

        std::unordered_map<std::tuple<std::type_info, int>, GLint> internalFormatMap;
        internalFormatMap[{typeid(float), 1}] = GL_R32F;
        internalFormatMap[{typeid(float), 2}] = GL_RG32F;
        internalFormatMap[{typeid(float), 3}] = GL_RGB32F;
        internalFormatMap[{typeid(float), 4}] = GL_RGBA32F;

        internalFormatMap[{typeid(int), 1}] = GL_R32I;
        internalFormatMap[{typeid(int), 2}] = GL_RG32I;
        internalFormatMap[{typeid(int), 3}] = GL_RGB32I;
        internalFormatMap[{typeid(int), 4}] = GL_RGBA32I;

        internalFormatMap[{typeid(unsigned char), 1}] = GL_R8;
        internalFormatMap[{typeid(unsigned char), 2}] = GL_RG8;
        internalFormatMap[{typeid(unsigned char), 3}] = GL_RGB8;
        internalFormatMap[{typeid(unsigned char), 4}] = GL_RGBA8;

        std::unordered_map<int, GLenum> formatMap;
        formatMap[1] = GL_RED;
        formatMap[2] = GL_RG;
        formatMap[3] = GL_RGB;
        formatMap[4] = GL_RGBA;

        std::unordered_map<std::type_info, GLenum> typeMap;
        formatMap[typeid(float)] = GL_FLOAT;
        formatMap[typeid(int)] = GL_INT;
        formatMap[typeid(unsigned char)] = GL_UNSIGNED_BYTE;

        internalFormat = internalFormatMap[{typeid(Type), channels}];
        format = formatMap[channels];
        type = typeMap[typeid(Type)];

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.width, image.height, 0, format, type, _nodata_value);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    dataGL(const dataGL &other)
    {
        nodata = other.nodata;
        width = other.width;
        height = other.height;
        channels = other.channels;

        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);

        // Attach the source texture to the FBO.
        glFramebufferTexture2D(
            GL_READ_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            other.textureID, // <-- your source texture handle
            0              // mipmap level
        );
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glCopyTexSubImage2D(
            GL_TEXTURE_2D, // target
            0,             // mipmap level
            0, 0,          // xoffset, yoffset in dest texture
            0, 0,          // x, y in the read buffer to start copying from
            width, height  // width, height to copy
        );
    }

    dataGL &operator=(const dataGL &other)
    {
        if (this != &other)
        {
            nodata = other.nodata;
            width = other.width;
            height = other.height;
            channels = other.channels;

            GLuint fbo;
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    
            // Attach the source texture to the FBO.
            glFramebufferTexture2D(
                GL_READ_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D,
                other.textureID, // <-- your source texture handle
                0              // mipmap level
            );
            glReadBuffer(GL_COLOR_ATTACHMENT0);
    
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glCopyTexSubImage2D(
                GL_TEXTURE_2D, // target
                0,             // mipmap level
                0, 0,          // xoffset, yoffset in dest texture
                0, 0,          // x, y in the read buffer to start copying from
                width, height  // width, height to copy
            );
        }
        return *this;
    }

    void set(Type *data)
    {
        std::memcpy(m_data, data, sizeof(Type) * channels * width * height);
    }

    /*
    ~data()
    {
        delete[] m_data;
        // m_data = nullptr;
    }
    */

    Type nodata;
    int width;
    int height;
    int channels;

private:
 
    GLuint textureID;
    GLint internalFormat;
    GLenum format;
    GLenum type;
};
