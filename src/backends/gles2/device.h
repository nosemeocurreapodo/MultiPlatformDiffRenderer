#pragma once

#include "glad/egl.h"
#include "glad/gles2.h"   // <-- GLES2 loader

inline bool InitEGL_GLES2()
{
    int m_windowWidth;
    int m_windowHeight;

    EGLBoolean success;
    EGLint num_configs;
    EGLConfig egl_config;
    EGLSurface egl_surface;
    EGLContext egl_context;
    EGLDisplay egl_display;

    m_windowWidth  = 256;
    m_windowHeight = 256;

    // ES2-capable config + pbuffer
    const EGLint egl_config_attribs[] = {
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,                 // alpha is often handy for offscreen
        EGL_DEPTH_SIZE,      8,                 // keep same as your GL path
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    const EGLint egl_pbuffer_attribs[] = {
        EGL_WIDTH,  m_windowWidth,
        EGL_HEIGHT, m_windowHeight,
        EGL_NONE
    };

    const EGLint egl_ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,          // <- ES 2.0
        EGL_NONE
    };

    EGLInternalData2* m_data = new EGLInternalData2();
    m_data->m_windowWidth  = m_windowWidth;
    m_data->m_windowHeight = m_windowHeight;
    m_data->m_renderDevice = -1;                // default: pick first working device

    // 1) Load EGL entry points
    int egl_version = gladLoaderLoadEGL(nullptr);
    if (!egl_version) {
        fprintf(stderr, "failed to load EGL with glad.\n");
        return false;
    }

    // 2) Enumerate devices (headless-friendly)
    const int max_devices = 32;
    EGLDeviceEXT egl_devices[max_devices];
    EGLint num_devices = 0;
    if (!eglQueryDevicesEXT(max_devices, egl_devices, &num_devices) ||
        eglGetError() != EGL_SUCCESS)
    {
        printf("eglQueryDevicesEXT failed; falling back to default display.\n");
        m_data->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    }

    // 3) Choose a display
    if (m_data->egl_display == EGL_NO_DISPLAY) {
        if (m_data->m_renderDevice == -1) {
            // Try all devices until one initializes
            for (EGLint i = 0; i < num_devices; ++i) {
                EGLDisplay display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT,
                                                              egl_devices[i], nullptr);
                if (eglGetError() == EGL_SUCCESS && display != EGL_NO_DISPLAY) {
                    EGLint major = 0, minor = 0;
                    EGLBoolean initialized = eglInitialize(display, &major, &minor);
                    if (eglGetError() == EGL_SUCCESS && initialized == EGL_TRUE) {
                        m_data->egl_display = display;
                        break;
                    }
                }
            }
        } else {
            if (m_data->m_renderDevice < 0 || m_data->m_renderDevice >= num_devices) {
                fprintf(stderr, "Invalid render_device: %d (num_devices=%d)\n",
                        m_data->m_renderDevice, num_devices);
                return false;
            }
            EGLDisplay display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT,
                                                          egl_devices[m_data->m_renderDevice], nullptr);
            if (eglGetError() == EGL_SUCCESS && display != EGL_NO_DISPLAY) {
                EGLint major = 0, minor = 0;
                EGLBoolean initialized = eglInitialize(display, &major, &minor);
                if (eglGetError() == EGL_SUCCESS && initialized == EGL_TRUE) {
                    m_data->egl_display = display;
                }
            }
        }
    }

    // Fallback if device enumeration path didn’t set a display
    if (m_data->egl_display == EGL_NO_DISPLAY) {
        m_data->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    }

    if (!eglInitialize(m_data->egl_display, nullptr, nullptr)) {
        fprintf(stderr, "Unable to initialize EGL\n");
        return false;
    }

    // Reload EGL with the chosen display (matches your GL path)
    egl_version = gladLoaderLoadEGL(m_data->egl_display);
    if (!egl_version) {
        fprintf(stderr, "Unable to reload EGL with display.\n");
        return false;
    }
    printf("Loaded EGL %d.%d after reload.\n",
           GLAD_VERSION_MAJOR(egl_version), GLAD_VERSION_MINOR(egl_version));

    // 4) Bind OpenGL ES API
    success = eglBindAPI(EGL_OPENGL_ES_API);
    if (!success) {
        fprintf(stderr, "Failed to bind OpenGL ES API.\n");
        return false;
    }

    // 5) Choose config
    success = eglChooseConfig(m_data->egl_display, egl_config_attribs,
                              &m_data->egl_config, 1, &m_data->num_configs);
    if (!success) {
        fprintf(stderr, "Failed to choose config (eglError: 0x%04x)\n", eglGetError());
        return false;
    }
    if (m_data->num_configs != 1) {
        fprintf(stderr, "Didn't get exactly one config, but %d\n", m_data->num_configs);
        return false;
    }

    // 6) Create pbuffer surface
    m_data->egl_surface = eglCreatePbufferSurface(m_data->egl_display,
                                                  m_data->egl_config,
                                                  egl_pbuffer_attribs);
    if (m_data->egl_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "Unable to create EGL pbuffer surface (eglError: 0x%04x)\n", eglGetError());
        return false;
    }

    // 7) Create ES 2.0 context
    m_data->egl_context = eglCreateContext(m_data->egl_display,
                                           m_data->egl_config,
                                           EGL_NO_CONTEXT,
                                           egl_ctx_attribs);
    if (m_data->egl_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "Unable to create EGL ES2 context (eglError: 0x%04x)\n", eglGetError());
        return false;
    }

    // 8) Make current
    success = eglMakeCurrent(m_data->egl_display,
                             m_data->egl_surface,
                             m_data->egl_surface,
                             m_data->egl_context);
    if (!success) {
        fprintf(stderr, "Failed to make ES2 context current (eglError: 0x%04x)\n", eglGetError());
        return false;
    }

    // 9) Load GLES2 function pointers
    if (!gladLoadGLES2((GLADloadfunc)eglGetProcAddress)) {
        fprintf(stderr, "failed to load GLES2 with glad.\n");
        return false;
    }

    return true;
}
