#pragma once

#include <cstdio>
#include <cstring>

#include "glad/egl.h"
#include "glad/gl.h"
#include "backends/base/egl_common.h"

// If your EGL headers don't expose these (some setups do, some don't)
#ifndef EGL_CONTEXT_MAJOR_VERSION_KHR
#define EGL_CONTEXT_MAJOR_VERSION_KHR 0x3098
#endif
#ifndef EGL_CONTEXT_MINOR_VERSION_KHR
#define EGL_CONTEXT_MINOR_VERSION_KHR 0x30FB
#endif
#ifndef EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR 0x30FD
#endif
#ifndef EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR 0x00000001
#endif

struct EGLInternalData2
{
    bool m_isInitialized = false;

    int m_windowWidth  = 0;
    int m_windowHeight = 0;
    int m_renderDevice = -1; // IMPORTANT: initialize this

    EGLBoolean success     = EGL_FALSE;
    EGLint num_configs     = 0;
    EGLConfig egl_config   = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLContext egl_context = EGL_NO_CONTEXT;
    EGLDisplay egl_display = EGL_NO_DISPLAY;
};

inline bool has_ext(const char* ext_list, const char* ext)
{
    if (!ext_list || !ext || !*ext) return false;

    // Exact token match in a space-separated list
    const char* p = ext_list;
    const size_t n = std::strlen(ext);
    while ((p = std::strstr(p, ext)) != nullptr)
    {
        const char before = (p == ext_list) ? ' ' : p[-1];
        const char after  = p[n];
        if ((before == ' ' || before == '\0') && (after == ' ' || after == '\0'))
            return true;
        p += n;
    }
    return false;
}

inline bool InitEGL()
{
    // --- desired defaults ---
    const int windowWidth  = 256;
    const int windowHeight = 256;
    const int renderDevice = -1; // -1 = pick first working device

    // Store into struct (fixes the uninitialized m_renderDevice bug)
    EGLInternalData2* m_data = new EGLInternalData2();
    m_data->m_windowWidth  = windowWidth;
    m_data->m_windowHeight = windowHeight;
    m_data->m_renderDevice = renderDevice;

    EGLint egl_config_attribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };

    EGLint egl_pbuffer_attribs[] = {
        EGL_WIDTH,  m_data->m_windowWidth,
        EGL_HEIGHT, m_data->m_windowHeight,
        EGL_NONE
    };

    // Load global EGL entry points (needed before querying devices/platform display)
    int egl_version = gladLoaderLoadEGL(nullptr);
    if (!egl_version)
    {
        std::fprintf(stderr, "Failed to load EGL (global) with glad.\n");
        return false;
    }

    // Clear any stale error
    eglGetError();

    // Query EGL devices (requires EGL_EXT_device_enumeration + EGL_EXT_platform_device)
    const int max_devices = 32;
    EGLDeviceEXT egl_devices[max_devices];
    EGLint num_devices = 0;

    if (!eglQueryDevicesEXT || !eglGetPlatformDisplayEXT)
    {
        std::fprintf(stderr, "Missing EGL_EXT_device_enumeration / EGL_EXT_platform_device entry points.\n");
        return false;
    }

    if (!eglQueryDevicesEXT(max_devices, egl_devices, &num_devices) || eglGetError() != EGL_SUCCESS || num_devices <= 0)
    {
        std::fprintf(stderr, "eglQueryDevicesEXT failed or found no devices.\n");
        return false;
    }

    // Pick a display
    m_data->egl_display = EGL_NO_DISPLAY;

    if (m_data->m_renderDevice == -1)
    {
        // Choose first device that successfully initializes (IMPORTANT: break after success)
        for (EGLint i = 0; i < num_devices; ++i)
        {
            EGLDisplay display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, egl_devices[i], nullptr);
            if (display == EGL_NO_DISPLAY) continue;

            EGLint major = 0, minor = 0;
            if (eglInitialize(display, &major, &minor) == EGL_TRUE)
            {
                m_data->egl_display = display;
                break;
            }
        }
    }
    else
    {
        if (m_data->m_renderDevice < 0 || m_data->m_renderDevice >= num_devices)
        {
            std::fprintf(stderr, "Invalid render device index %d (num_devices=%d)\n", m_data->m_renderDevice, num_devices);
            return false;
        }

        EGLDisplay display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT,
                                                     egl_devices[m_data->m_renderDevice], nullptr);
        if (display == EGL_NO_DISPLAY)
        {
            std::fprintf(stderr, "eglGetPlatformDisplayEXT returned EGL_NO_DISPLAY.\n");
            return false;
        }

        EGLint major = 0, minor = 0;
        if (eglInitialize(display, &major, &minor) != EGL_TRUE)
        {
            std::fprintf(stderr, "eglInitialize failed for selected device.\n");
            return false;
        }

        m_data->egl_display = display;
    }

    if (m_data->egl_display == EGL_NO_DISPLAY)
    {
        std::fprintf(stderr, "No EGL display could be initialized.\n");
        return false;
    }

    // Reload EGL with the selected display (so display-dependent funcs are good)
    egl_version = gladLoaderLoadEGL(m_data->egl_display);
    if (!egl_version)
    {
        std::fprintf(stderr, "Unable to reload EGL for display.\n");
        return false;
    }
    std::printf("Loaded EGL %d.%d after reload.\n", GLAD_VERSION_MAJOR(egl_version), GLAD_VERSION_MINOR(egl_version));

    // Bind desktop OpenGL (not GLES)
    m_data->success = eglBindAPI(EGL_OPENGL_API);
    if (!m_data->success)
    {
        std::fprintf(stderr, "Failed to bind EGL_OPENGL_API (eglError=%#x)\n", eglGetError());
        return false;
    }

    // Choose config
    m_data->success = eglChooseConfig(m_data->egl_display, egl_config_attribs,
                                     &m_data->egl_config, 1, &m_data->num_configs);
    if (!m_data->success || m_data->num_configs != 1)
    {
        std::fprintf(stderr, "Failed to choose EGL config (success=%d, num=%d, eglError=%#x)\n",
                     (int)m_data->success, (int)m_data->num_configs, eglGetError());
        return false;
    }

    // Create pbuffer surface
    m_data->egl_surface = eglCreatePbufferSurface(m_data->egl_display, m_data->egl_config, egl_pbuffer_attribs);
    if (m_data->egl_surface == EGL_NO_SURFACE)
    {
        std::fprintf(stderr, "Unable to create EGL pbuffer surface (eglError=%#x)\n", eglGetError());
        return false;
    }

    // --- Create an OpenGL 4.6 Core context (requires EGL_KHR_create_context) ---
    const char* exts = eglQueryString(m_data->egl_display, EGL_EXTENSIONS);
    const bool has_khr_create = has_ext(exts, "EGL_KHR_create_context");

    EGLContext ctx = EGL_NO_CONTEXT;

    if (has_khr_create)
    {
        EGLint ctx_attribs_46[] = {
            EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
            EGL_CONTEXT_MINOR_VERSION_KHR, 6,
            EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
            EGL_NONE
        };

        ctx = eglCreateContext(m_data->egl_display, m_data->egl_config, EGL_NO_CONTEXT, ctx_attribs_46);

        // Optional fallback: try 4.5 if 4.6 fails
        if (ctx == EGL_NO_CONTEXT)
        {
            EGLint ctx_attribs_45[] = {
                EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
                EGL_CONTEXT_MINOR_VERSION_KHR, 5,
                EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
                EGL_NONE
            };
            ctx = eglCreateContext(m_data->egl_display, m_data->egl_config, EGL_NO_CONTEXT, ctx_attribs_45);
        }
    }
    else
    {
        std::fprintf(stderr, "Warning: EGL_KHR_create_context not available; creating context without version request.\n");
        ctx = eglCreateContext(m_data->egl_display, m_data->egl_config, EGL_NO_CONTEXT, nullptr);
    }

    m_data->egl_context = ctx;
    if (m_data->egl_context == EGL_NO_CONTEXT)
    {
        std::fprintf(stderr, "Unable to create EGL context (eglError=%#x)\n", eglGetError());
        return false;
    }

    // Make current
    m_data->success = eglMakeCurrent(m_data->egl_display, m_data->egl_surface, m_data->egl_surface, m_data->egl_context);
    if (!m_data->success)
    {
        std::fprintf(stderr, "Failed to make context current (eglError=%#x)\n", eglGetError());
        return false;
    }

    // Load GL functions
    if (!gladLoadGL(eglGetProcAddress))
    {
        std::fprintf(stderr, "Failed to load GL with glad.\n");
        return false;
    }

    // Print what you *actually* got
    std::printf("GL_VERSION: %s\n", (const char*)glGetString(GL_VERSION));
    std::printf("GLSL:      %s\n", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Your existing helper (if it prints useful extra info)
    PrintEGLAndGLInfo(m_data->egl_display, m_data->egl_context);

    return true;
}
