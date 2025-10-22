#pragma once

#include "glad/egl.h"
#include "glad/gles2.h" // was glad/gl.h

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
          m_windowHeight(0),
          m_renderDevice(-1) {} // initialize this (you use it below)
};

inline bool InitEGL_GLES2() // new name to avoid confusion
{
    int m_windowWidth = 256;
    int m_windowHeight = 256;

    EGLInternalData2 *m_data = new EGLInternalData2();

    const EGLint egl_config_attribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, // was EGL_OPENGL_BIT
        EGL_NONE};

    const EGLint egl_pbuffer_attribs[] = {
        EGL_WIDTH, m_windowWidth,
        EGL_HEIGHT, m_windowHeight,
        EGL_NONE};

    const EGLint egl_ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, // ES 2.0 context
        EGL_NONE};

    // 1) Load EGL entry points (glad2-style)
    int egl_version = gladLoaderLoadEGL(EGL_NO_DISPLAY);
    if (!egl_version)
    {
        fprintf(stderr, "Failed to load EGL with glad.\n");
        return false;
    }

    // 2) Enumerate devices (requires EGL_EXT_device_enumeration etc.)
    const int max_devices = 32;
    EGLDeviceEXT egl_devices[max_devices];
    EGLint num_devices = 0;
    if (!eglQueryDevicesEXT(max_devices, egl_devices, &num_devices))
    {
        printf("eglQueryDevicesEXT failed; falling back to default display.\n");
        m_data->egl_display = EGL_NO_DISPLAY;
    }

    // 3) Pick a display (try devices first)
    if (m_data->m_renderDevice == -1)
    {
        for (EGLint i = 0; i < num_devices; ++i)
        {
            EGLDisplay display =
                eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, egl_devices[i], nullptr);
            if (eglGetError() == EGL_SUCCESS && display != EGL_NO_DISPLAY)
            {
                EGLint major = 0, minor = 0;
                if (eglInitialize(display, &major, &minor) == EGL_TRUE &&
                    eglGetError() == EGL_SUCCESS)
                {
                    m_data->egl_display = display;
                    break;
                }
            }
        }
    }
    else
    {
        if (m_data->m_renderDevice < 0 || m_data->m_renderDevice >= num_devices)
        {
            fprintf(stderr, "Invalid render_device choice: %d < %d.\n",
                    m_data->m_renderDevice, num_devices);
            return false;
        }
        EGLDisplay display =
            eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT,
                                     egl_devices[m_data->m_renderDevice], nullptr);
        if (eglGetError() == EGL_SUCCESS && display != EGL_NO_DISPLAY)
        {
            EGLint major = 0, minor = 0;
            if (eglInitialize(display, &major, &minor) == EGL_TRUE &&
                eglGetError() == EGL_SUCCESS)
            {
                m_data->egl_display = display;
            }
        }
    }

    if (m_data->egl_display == EGL_NO_DISPLAY)
    {
        m_data->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    }
    if (!eglInitialize(m_data->egl_display, nullptr, nullptr))
    {
        fprintf(stderr, "Unable to initialize EGL\n");
        return false;
    }

    // 4) Reload EGL against the chosen display (glad2)
    egl_version = gladLoaderLoadEGL(m_data->egl_display);
    if (!egl_version)
    {
        fprintf(stderr, "Unable to reload EGL.\n");
        return false;
    }
    printf("Loaded EGL %d.%d after reload.\n",
           GLAD_VERSION_MAJOR(egl_version), GLAD_VERSION_MINOR(egl_version));

    // 5) Bind the **OpenGL ES** API (not desktop GL)
    m_data->success = eglBindAPI(EGL_OPENGL_ES_API); // was EGL_OPENGL_API
    if (!m_data->success)
    {
        fprintf(stderr, "Failed to bind OpenGL ES API.\n");
        return false;
    }

    // 6) Choose ES-capable config
    m_data->success = eglChooseConfig(m_data->egl_display, egl_config_attribs,
                                      &m_data->egl_config, 1, &m_data->num_configs);
    if (!m_data->success || m_data->num_configs != 1)
    {
        fprintf(stderr, "Failed to choose config (eglError: 0x%04x)\n", eglGetError());
        return false;
    }

    // 7) Create pbuffer
    m_data->egl_surface = eglCreatePbufferSurface(
        m_data->egl_display, m_data->egl_config, egl_pbuffer_attribs);
    if (m_data->egl_surface == EGL_NO_SURFACE)
    {
        fprintf(stderr, "Unable to create EGL surface (eglError: 0x%04x)\n", eglGetError());
        return false;
    }

    // 8) Create **ES 2.0** context
    m_data->egl_context = eglCreateContext(
        m_data->egl_display, m_data->egl_config, EGL_NO_CONTEXT, egl_ctx_attribs); // was NULL attribs
    if (m_data->egl_context == EGL_NO_CONTEXT)
    {
        fprintf(stderr, "Unable to create EGL ES2 context (eglError: 0x%04x)\n", eglGetError());
        return false;
    }

    // 9) Make current
    m_data->success = eglMakeCurrent(m_data->egl_display,
                                     m_data->egl_surface, m_data->egl_surface,
                                     m_data->egl_context);
    if (!m_data->success)
    {
        fprintf(stderr, "Failed to make context current (eglError: 0x%04x)\n", eglGetError());
        return false;
    }

    // 10) Load **GLES2** function pointers (not desktop GL)
    if (!gladLoadGLES2((GLADloadfunc)eglGetProcAddress))
    {
        fprintf(stderr, "Failed to load GLES2 with glad.\n");
        return false;
    }

    return true;
}
