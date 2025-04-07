#pragma once

//#include <GL/glew.h>
#include <GLFW/glfw3.h>
//#include <pangolin/pangolin.h>
#include <EGL/egl.h>
// #include <GLES2/gl2.h>
#include <GL/gl.h> // if using desktop OpenGL, depending on your setup

#include "common/types.h"

EGLDisplay eglDisplay;
EGLContext eglContext;
EGLSurface eglSurface;

inline bool initEGL()
{
    // 1. Get an EGL display
    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY)
    {
        // handle error
        return false;
    }

    // 2. Initialize EGL
    if (!eglInitialize(eglDisplay, nullptr, nullptr))
    {
        // handle error
        return false;
    }

    // 3. Choose an EGL config
    EGLint attr[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, // or EGL_OPENGL_ES2_BIT, etc.
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,   // offscreen
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE};
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(eglDisplay, attr, &config, 1, &numConfigs);

    // 4. Create a PBuffer surface (offscreen)
    EGLint pbufferAttribs[] = {
        EGL_WIDTH, 800,
        EGL_HEIGHT, 600,
        EGL_NONE};
    eglSurface = eglCreatePbufferSurface(eglDisplay, config, pbufferAttribs);
    if (eglSurface == EGL_NO_SURFACE)
    {
        // handle error
        return false;
    }

    // 5. Bind the correct API
    eglBindAPI(EGL_OPENGL_API); // or EGL_OPENGL_ES_API

    // 6. Create an EGL context
    EGLint ctxAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 4, // or 3, 2, etc. depending on what's supported
        EGL_CONTEXT_MINOR_VERSION, 5,
        EGL_NONE};
    eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
    if (eglContext == EGL_NO_CONTEXT)
    {
        // handle error
        return false;
    }

    // 7. Make the context current
    if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext))
    {
        // handle error
        return false;
    }

    if(!glfwInit())
    {
        return false;
    } 

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    return true;
}

template <typename Type>
class dataGL
{
public:
    dataGL(int _width, int _height, int _channels, Type _nodata_value)
    {
        if (_channels > 4)
            throw std::runtime_error("Too many channels in dataGL");

        nodata = _nodata_value;
        width = _width;
        height = _height;
        channels = _channels;

        std::map<std::pair<std::type_index, int>, GLint> internalFormatMap;
        internalFormatMap[std::make_pair(get_type_index<float>(), 1)] = GL_R32F;
        internalFormatMap[std::make_pair(get_type_index<float>(), 2)] = GL_RG32F;
        internalFormatMap[std::make_pair(get_type_index<float>(), 3)] = GL_RGB32F;
        internalFormatMap[std::make_pair(get_type_index<float>(), 4)] = GL_RGBA32F;

        internalFormatMap[std::make_pair(get_type_index<int>(), 1)] = GL_R32I;
        internalFormatMap[std::make_pair(get_type_index<int>(), 2)] = GL_RG32I;
        internalFormatMap[std::make_pair(get_type_index<int>(), 3)] = GL_RGB32I;
        internalFormatMap[std::make_pair(get_type_index<int>(), 4)] = GL_RGBA32I;

        internalFormatMap[std::make_pair(get_type_index<unsigned char>(), 1)] = GL_R8;
        internalFormatMap[std::make_pair(get_type_index<unsigned char>(), 2)] = GL_RG8;
        internalFormatMap[std::make_pair(get_type_index<unsigned char>(), 3)] = GL_RGB8;
        internalFormatMap[std::make_pair(get_type_index<unsigned char>(), 4)] = GL_RGBA8;

        std::map<int, GLenum> formatMap;
        formatMap[1] = GL_RED;
        formatMap[2] = GL_RG;
        formatMap[3] = GL_RGB;
        formatMap[4] = GL_RGBA;

        std::map<std::type_index, GLenum> typeMap;
        typeMap[get_type_index<float>()] = GL_FLOAT;
        typeMap[get_type_index<int>()] = GL_INT;
        typeMap[get_type_index<unsigned char>()] = GL_UNSIGNED_BYTE;

        internalFormat = internalFormatMap[std::make_pair(get_type_index<Type>(), channels)];
        format = formatMap[channels];
        type = typeMap[get_type_index<Type>()];

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr);
        // glGenerateMipmap(GL_TEXTURE_2D);
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
            0                // mipmap level
        );
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glCopyTexSubImage2D(
            GL_TEXTURE_2D,
            0,            // mipmap level
            0, 0,         // xoffset, yoffset in dest texture
            0, 0,         // x, y in the read buffer to start copying from
            width, height // width, height to copy
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
                0                // mipmap level
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
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    void toCPU(Type *data)
    {
        glBindTexture(GL_TEXTURE_2D, textureID);
        glGetTexImage(GL_TEXTURE_2D, 0, format, type, data);
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
