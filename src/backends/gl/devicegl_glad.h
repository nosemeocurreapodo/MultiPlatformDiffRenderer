#pragma once

#include "glad/egl.h"
#include "glad/gl.h"

#include <map>
#include "core/format_converters.h"

struct EGLInternalData2
{
    bool m_isInitialized;

    int m_windowWidth;
    int m_windowHeight;
    int m_renderDevice;

    EGLBoolean success;
    EGLint num_configs;
    EGLConfig egl_config;
    EGLSurface egl_surface;
    EGLContext egl_context;
    EGLDisplay egl_display;

    EGLInternalData2()
        : m_isInitialized(false),
          m_windowWidth(0),
          m_windowHeight(0) {}
};

inline bool InitEGL()
{
    int m_windowWidth;
    int m_windowHeight;
    int m_renderDevice;

    EGLBoolean success;
    EGLint num_configs;
    EGLConfig egl_config;
    EGLSurface egl_surface;
    EGLContext egl_context;
    EGLDisplay egl_display;

    m_windowWidth = 256;
    m_windowHeight = 256;
    m_renderDevice = -1;

    EGLint egl_config_attribs[] = {EGL_RED_SIZE,
                                   8,
                                   EGL_GREEN_SIZE,
                                   8,
                                   EGL_BLUE_SIZE,
                                   8,
                                   EGL_DEPTH_SIZE,
                                   8,
                                   EGL_SURFACE_TYPE,
                                   EGL_PBUFFER_BIT,
                                   EGL_RENDERABLE_TYPE,
                                   EGL_OPENGL_BIT,
                                   EGL_NONE};

    EGLint egl_pbuffer_attribs[] = {
        EGL_WIDTH,
        m_windowWidth,
        EGL_HEIGHT,
        m_windowHeight,
        EGL_NONE,
    };

    EGLInternalData2 *m_data = new EGLInternalData2();

    // Load EGL functions
    int egl_version = gladLoaderLoadEGL(NULL);
    if (!egl_version)
    {
        fprintf(stderr, "failed to EGL with glad.\n");
        return false;
    };

    // Query EGL Devices
    const int max_devices = 32;
    EGLDeviceEXT egl_devices[max_devices];
    EGLint num_devices = 0;
    EGLint egl_error = eglGetError();
    if (!eglQueryDevicesEXT(max_devices, egl_devices, &num_devices) ||
        egl_error != EGL_SUCCESS)
    {
        printf("eglQueryDevicesEXT Failed.\n");
        m_data->egl_display = EGL_NO_DISPLAY;
    }

    // Query EGL Screens
    if (m_data->m_renderDevice == -1)
    {
        // Chose default screen, by trying all
        for (EGLint i = 0; i < num_devices; ++i)
        {
            // Set display
            EGLDisplay display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT,
                                                          egl_devices[i], NULL);
            if (eglGetError() == EGL_SUCCESS && display != EGL_NO_DISPLAY)
            {
                int major, minor;
                EGLBoolean initialized = eglInitialize(display, &major, &minor);
                if (eglGetError() == EGL_SUCCESS && initialized == EGL_TRUE)
                {
                    m_data->egl_display = display;
                }
            }
        }
    }
    else
    {
        // Chose specific screen, by using m_renderDevice
        if (m_data->m_renderDevice < 0 || m_data->m_renderDevice >= num_devices)
        {
            fprintf(stderr, "Invalid render_device choice: %d < %d.\n", m_data->m_renderDevice, num_devices);
            return false;
        }

        // Set display
        EGLDisplay display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT,
                                                      egl_devices[m_data->m_renderDevice], NULL);
        if (eglGetError() == EGL_SUCCESS && display != EGL_NO_DISPLAY)
        {
            int major, minor;
            EGLBoolean initialized = eglInitialize(display, &major, &minor);
            if (eglGetError() == EGL_SUCCESS && initialized == EGL_TRUE)
            {
                m_data->egl_display = display;
            }
        }
    }

    if (!eglInitialize(m_data->egl_display, NULL, NULL))
    {
        fprintf(stderr, "Unable to initialize EGL\n");
        return false;
    }

    egl_version = gladLoaderLoadEGL(m_data->egl_display);
    if (!egl_version)
    {
        fprintf(stderr, "Unable to reload EGL.\n");
        return false;
    }
    printf("Loaded EGL %d.%d after reload.\n", GLAD_VERSION_MAJOR(egl_version),
           GLAD_VERSION_MINOR(egl_version));

    m_data->success = eglBindAPI(EGL_OPENGL_API);
    if (!m_data->success)
    {
        // TODO: Properly handle this error (requires change to default window
        // API to change return on all window types to bool).
        fprintf(stderr, "Failed to bind OpenGL API.\n");
        return false;
    }

    m_data->success =
        eglChooseConfig(m_data->egl_display, egl_config_attribs,
                        &m_data->egl_config, 1, &m_data->num_configs);
    if (!m_data->success)
    {
        // TODO: Properly handle this error (requires change to default window
        // API to change return on all window types to bool).
        fprintf(stderr, "Failed to choose config (eglError: %d)\n", eglGetError());
        return false;
    }
    if (m_data->num_configs != 1)
    {
        fprintf(stderr, "Didn't get exactly one config, but %d\n", m_data->num_configs);
        return false;
    }

    m_data->egl_surface = eglCreatePbufferSurface(
        m_data->egl_display, m_data->egl_config, egl_pbuffer_attribs);
    if (m_data->egl_surface == EGL_NO_SURFACE)
    {
        fprintf(stderr, "Unable to create EGL surface (eglError: %d)\n", eglGetError());
        return false;
    }

    m_data->egl_context = eglCreateContext(
        m_data->egl_display, m_data->egl_config, EGL_NO_CONTEXT, NULL);
    if (!m_data->egl_context)
    {
        fprintf(stderr, "Unable to create EGL context (eglError: %d)\n", eglGetError());
        return false;
    }

    m_data->success =
        eglMakeCurrent(m_data->egl_display, m_data->egl_surface, m_data->egl_surface,
                       m_data->egl_context);
    if (!m_data->success)
    {
        fprintf(stderr, "Failed to make context current (eglError: %d)\n", eglGetError());
        return false;
    }

    if (!gladLoadGL(eglGetProcAddress))
    {
        fprintf(stderr, "failed to load GL with glad.\n");
        return false;
    }

    return true;
}

template <typename T>
inline int getChannels()
{
    if constexpr (std::is_same_v<T, Scalar>)
    {
        return 1;
    }
    else if constexpr (std::is_same_v<T, Vec2>)
    {
        return 2;
    }
    else if constexpr (std::is_same_v<T, Vec3>)
    {
        return 3;
    }
    else if constexpr (std::is_same_v<T, Vec4>)
    {
        return 4;
    }
    else
    {
        // static_assert(false, "Unsupported type for getChannels");
        // assert(false);
        return 0; // Unreachable
    }
}

inline GLint GetGLInternalFormat(std::type_index tindex)
{
    // static std::map<std::pair<std::type_index, int>, GLint> internal_format_map;
    static std::map<std::type_index, GLint> internal_format_map;
    // internal_format_map[GetTypeIndex<float>()] = GL_R32F;
    // internal_format_map[GetTypeIndex<ImageType>()] = GL_R32F;
    internal_format_map[GetTypeIndex<Scalar>()] = GL_R32F;
    internal_format_map[GetTypeIndex<Vec2>()] = GL_RG32F;
    internal_format_map[GetTypeIndex<Vec3>()] = GL_RGB32F;
    internal_format_map[GetTypeIndex<Vec4>()] = GL_RGBA32F;

    internal_format_map[GetTypeIndex<Int>()] = GL_R32I;
    internal_format_map[GetTypeIndex<Vec2i>()] = GL_RG32I;
    internal_format_map[GetTypeIndex<Vec3i>()] = GL_RGB32I;
    internal_format_map[GetTypeIndex<Vec4i>()] = GL_RGBA32I;

    internal_format_map[GetTypeIndex<unsigned char>()] = GL_R8;
    // internal_format_map[std::make_pair(GetTypeIndex<unsigned char>(), 2)] = GL_RG8;
    // internal_format_map[std::make_pair(GetTypeIndex<unsigned char>(), 3)] = GL_RGB8;
    // internal_format_map[std::make_pair(GetTypeIndex<unsigned char>(), 4)] = GL_RGBA8;

    // return internal_format_map[std::make_pair(tindex, channels)];
    return internal_format_map[tindex];
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
    // type_map[GetTypeIndex<float>()] = GL_FLOAT;
    // type_map[GetTypeIndex<ImageType>()] = GL_FLOAT;
    type_map[GetTypeIndex<Scalar>()] = GL_FLOAT;
    type_map[GetTypeIndex<Vec2>()] = GL_FLOAT;
    type_map[GetTypeIndex<Vec3>()] = GL_FLOAT;
    type_map[GetTypeIndex<Vec4>()] = GL_FLOAT;
    type_map[GetTypeIndex<Int>()] = GL_INT;
    type_map[GetTypeIndex<Vec2i>()] = GL_INT;
    type_map[GetTypeIndex<Vec2i>()] = GL_INT;
    type_map[GetTypeIndex<Vec2i>()] = GL_INT;
    type_map[GetTypeIndex<unsigned char>()] = GL_UNSIGNED_BYTE;

    return type_map[tindex];
}
