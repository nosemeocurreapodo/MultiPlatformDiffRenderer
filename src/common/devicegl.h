#pragma once

#include <GL/glew.h>
// #include <GLFW/glfw3.h>
// #include <pangolin/pangolin.h>
#include <EGL/egl.h>
// #include <GLES2/gl2.h>
#include <GL/gl.h> // if using desktop OpenGL, depending on your setup

#include <map>
#include "common/common.h"

EGLDisplay eglDisplay;
EGLContext eglContext;
EGLSurface eglSurface;

inline bool InitEGL()
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

    if (glewInit() != GLEW_OK)
    {
        return false;
    }

    return true;
}

inline GLint GetGLInternalFormat(std::type_index tindex, int channels)
{
    static std::map<std::pair<std::type_index, int>, GLint> internal_format_map;
    internal_format_map[std::make_pair(GetTypeIndex<float>(), 1)] = GL_R32F;
    internal_format_map[std::make_pair(GetTypeIndex<float>(), 2)] = GL_RG32F;
    internal_format_map[std::make_pair(GetTypeIndex<float>(), 3)] = GL_RGB32F;
    internal_format_map[std::make_pair(GetTypeIndex<float>(), 4)] = GL_RGBA32F;

    internal_format_map[std::make_pair(GetTypeIndex<int>(), 1)] = GL_R32I;
    internal_format_map[std::make_pair(GetTypeIndex<int>(), 2)] = GL_RG32I;
    internal_format_map[std::make_pair(GetTypeIndex<int>(), 3)] = GL_RGB32I;
    internal_format_map[std::make_pair(GetTypeIndex<int>(), 4)] = GL_RGBA32I;

    internal_format_map[std::make_pair(GetTypeIndex<unsigned char>(), 1)] = GL_R8;
    internal_format_map[std::make_pair(GetTypeIndex<unsigned char>(), 2)] = GL_RG8;
    internal_format_map[std::make_pair(GetTypeIndex<unsigned char>(), 3)] = GL_RGB8;
    internal_format_map[std::make_pair(GetTypeIndex<unsigned char>(), 4)] = GL_RGBA8;

    return internal_format_map[std::make_pair(tindex, channels)];
}

inline GLenum GetGLFormat(int channels)
{
    static std::map<int, GLenum> format_map;
    format_map[1] = GL_RED;
    format_map[2] = GL_RG;
    format_map[3] = GL_RGB;
    format_map[4] = GL_RGBA;

    return format_map[channels];
}

inline GLenum GetGLType(std::type_index tindex)
{
    static std::map<std::type_index, GLenum> type_map;
    type_map[GetTypeIndex<float>()] = GL_FLOAT;
    type_map[GetTypeIndex<int>()] = GL_INT;
    type_map[GetTypeIndex<unsigned char>()] = GL_UNSIGNED_BYTE;

    return type_map[tindex];
}
