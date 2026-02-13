#pragma once

#include <cstdio>
#include <cstring>

#include "glad/egl.h"
#include "glad/gl.h"
#include "backends/base/egl_common.h"

// Some headers don't define these depending on EGL header version.
// Keep it safe and explicit.
#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif

namespace eglutil
{
    inline const char *EglErrorToString(EGLint err)
    {
        switch (err)
        {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "EGL_<unknown>";
        }
    }

    inline bool HasExtension(const char *extList, const char *ext)
    {
        if (!extList || !ext || !*ext)
            return false;
        const size_t extLen = std::strlen(ext);

        const char *start = extList;
        while (true)
        {
            const char *where = std::strstr(start, ext);
            if (!where)
                return false;

            const char *before = (where == extList) ? nullptr : where - 1;
            const char *after = where + extLen;

            const bool okBefore = (!before) || (*before == ' ');
            const bool okAfter = (*after == '\0') || (*after == ' ');

            if (okBefore && okAfter)
                return true;
            start = after;
        }
    }

    inline void PrintEglFailure(const char *what)
    {
        const EGLint err = eglGetError();
        std::fprintf(stderr, "%s (eglGetError=%s / 0x%04x)\n", what, EglErrorToString(err), err);
    }

    // Pick an EGLDisplay. Uses device platform if available, otherwise falls back to default display.
    inline bool PickDisplay(EGLDisplay &outDisplay, int renderDevice)
    {
        outDisplay = EGL_NO_DISPLAY;

        const bool haveDevicePlatform =
            (eglQueryDevicesEXT != nullptr) && (eglGetPlatformDisplayEXT != nullptr);

        if (haveDevicePlatform)
        {
            constexpr int kMaxDevices = 32;
            EGLDeviceEXT devices[kMaxDevices];
            EGLint numDevices = 0;

            if (!eglQueryDevicesEXT(kMaxDevices, devices, &numDevices))
            {
                PrintEglFailure("eglQueryDevicesEXT failed");
                numDevices = 0;
            }

            if (numDevices > 0)
            {
                auto try_device = [&](int idx) -> bool
                {
                    EGLDisplay dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, devices[idx], nullptr);
                    if (dpy == EGL_NO_DISPLAY)
                        return false;

                    EGLint major = 0, minor = 0;
                    if (eglInitialize(dpy, &major, &minor) != EGL_TRUE)
                        return false;

                    outDisplay = dpy;
                    return true;
                };

                if (renderDevice >= 0)
                {
                    if (renderDevice >= numDevices)
                    {
                        std::fprintf(stderr, "Invalid renderDevice=%d (numDevices=%d)\n", renderDevice, numDevices);
                        return false;
                    }
                    if (try_device(renderDevice))
                        return true;
                    PrintEglFailure("Failed to initialize requested EGL device display");
                    return false;
                }

                for (int i = 0; i < numDevices; ++i)
                    if (try_device(i))
                        return true;

                PrintEglFailure("No EGL device display could be initialized");
                // Fall through to default display fallback
            }
        }

        EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (dpy == EGL_NO_DISPLAY)
        {
            PrintEglFailure("eglGetDisplay(EGL_DEFAULT_DISPLAY) returned EGL_NO_DISPLAY");
            return false;
        }

        EGLint major = 0, minor = 0;
        if (eglInitialize(dpy, &major, &minor) != EGL_TRUE)
        {
            PrintEglFailure("eglInitialize(default display) failed");
            return false;
        }

        outDisplay = dpy;
        return true;
    }

} // namespace eglutil

struct EGLInternalData2
{
    enum class API
    {
        OpenGL,
        OpenGLES
    };

    bool m_isInitialized = false;

    int m_windowWidth = 0;
    int m_windowHeight = 0;
    int m_renderDevice = -1;

    API m_api = API::OpenGL;
    int m_glesClientVersion = 0; // 3 or 2 when using OpenGLES

    EGLBoolean success = EGL_FALSE;
    EGLint num_configs = 0;
    EGLConfig egl_config = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLContext egl_context = EGL_NO_CONTEXT;
    EGLDisplay egl_display = EGL_NO_DISPLAY;

    void Destroy()
    {
        if (egl_display != EGL_NO_DISPLAY)
        {
            eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

            if (egl_context != EGL_NO_CONTEXT)
            {
                eglDestroyContext(egl_display, egl_context);
                egl_context = EGL_NO_CONTEXT;
            }
            if (egl_surface != EGL_NO_SURFACE)
            {
                eglDestroySurface(egl_display, egl_surface);
                egl_surface = EGL_NO_SURFACE;
            }

            eglTerminate(egl_display);
            egl_display = EGL_NO_DISPLAY;
        }

        m_isInitialized = false;
        m_api = API::OpenGL;
        m_glesClientVersion = 0;
        egl_config = nullptr;
        num_configs = 0;
    }
};

// Internal attempt for a specific API.
// Returns true on success; on failure it leaves resources in a valid Destroy()-able state.
inline bool InitEGLTry(EGLInternalData2 &out,
                       EGLInternalData2::API api,
                       int width,
                       int height,
                       int renderDevice,
                       int glMajor,
                       int glMinor,
                       int glesClientVersion /*3 or 2*/)
{
    out.Destroy();

    out.m_windowWidth = width;
    out.m_windowHeight = height;
    out.m_renderDevice = renderDevice;
    out.m_api = api;
    out.m_glesClientVersion = (api == EGLInternalData2::API::OpenGLES) ? glesClientVersion : 0;

    // Load EGL entry points (enough to query devices / platform display).
    const int egl_version0 = gladLoaderLoadEGL(nullptr);
    if (!egl_version0)
    {
        std::fprintf(stderr, "GLAD failed to load EGL entry points.\n");
        return false;
    }

    if (!eglutil::PickDisplay(out.egl_display, out.m_renderDevice))
        return false;

    // Reload EGL with selected display
    const int egl_version1 = gladLoaderLoadEGL(out.egl_display);
    if (!egl_version1)
    {
        std::fprintf(stderr, "GLAD failed to reload EGL with selected display.\n");
        out.Destroy();
        return false;
    }

    // Bind correct API
    const EGLenum eglApi = (api == EGLInternalData2::API::OpenGL) ? EGL_OPENGL_API : EGL_OPENGL_ES_API;
    out.success = eglBindAPI(eglApi);
    if (out.success != EGL_TRUE)
    {
        eglutil::PrintEglFailure("eglBindAPI failed");
        out.Destroy();
        return false;
    }

    // Config: choose renderable type based on API (and ES version)
    EGLint renderable = 0;
    if (api == EGLInternalData2::API::OpenGL)
        renderable = EGL_OPENGL_BIT;
    else
        renderable = (glesClientVersion >= 3) ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_ES2_BIT;

    const EGLint egl_config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, renderable,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE};

    out.success = eglChooseConfig(out.egl_display, egl_config_attribs,
                                  &out.egl_config, 1, &out.num_configs);
    if (out.success != EGL_TRUE || out.num_configs != 1)
    {
        eglutil::PrintEglFailure("eglChooseConfig failed (or did not return exactly 1 config)");
        out.Destroy();
        return false;
    }

    const EGLint egl_pbuffer_attribs[] = {
        EGL_WIDTH, out.m_windowWidth,
        EGL_HEIGHT, out.m_windowHeight,
        EGL_NONE};

    out.egl_surface = eglCreatePbufferSurface(out.egl_display, out.egl_config, egl_pbuffer_attribs);
    if (out.egl_surface == EGL_NO_SURFACE)
    {
        eglutil::PrintEglFailure("eglCreatePbufferSurface failed");
        out.Destroy();
        return false;
    }

    // const char *apis = eglQueryString(out.egl_display, EGL_CLIENT_APIS);
    // std::printf("EGL_CLIENT_APIS: %s\n", apis ? apis : "(null)");

    // Create context attributes
    const char *exts = eglQueryString(out.egl_display, EGL_EXTENSIONS);
    // std::printf("EGL_EXTENSIONS:  %s\n", exts ? exts : "(null)");

    const bool hasKHRCreateContext = eglutil::HasExtension(exts, "EGL_KHR_create_context");

    // Desktop GL: try core profile if possible; otherwise nullptr fallback.
    EGLint ctx_attribs_gl_core[] = {
        EGL_CONTEXT_MAJOR_VERSION, glMajor,
        EGL_CONTEXT_MINOR_VERSION, glMinor,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE};

    // GLES: use EGL_CONTEXT_CLIENT_VERSION (portable).
    EGLint ctx_attribs_gles[] = {
        EGL_CONTEXT_CLIENT_VERSION, glesClientVersion,
        EGL_NONE};

    const EGLint *ctxAttribs = nullptr;
    if (api == EGLInternalData2::API::OpenGL)
        ctxAttribs = hasKHRCreateContext ? ctx_attribs_gl_core : nullptr;
    else
        ctxAttribs = ctx_attribs_gles;

    out.egl_context = eglCreateContext(out.egl_display, out.egl_config, EGL_NO_CONTEXT, ctxAttribs);
    if (out.egl_context == EGL_NO_CONTEXT)
    {
        eglutil::PrintEglFailure("eglCreateContext failed");
        out.Destroy();
        return false;
    }

    out.success = eglMakeCurrent(out.egl_display, out.egl_surface, out.egl_surface, out.egl_context);
    if (out.success != EGL_TRUE)
    {
        eglutil::PrintEglFailure("eglMakeCurrent failed");
        out.Destroy();
        return false;
    }

    // Load GL/GLES symbols
    if (api == EGLInternalData2::API::OpenGL)
    {
        if (!gladLoadGL(eglGetProcAddress))
        {
            std::fprintf(stderr, "GLAD failed to load desktop GL entry points.\n");
            out.Destroy();
            return false;
        }
    }
    else
    {
// GLAD naming is commonly gladLoadGLES2 even for ES3.
// If your GLAD was generated without GLES, you'll hit this compile-time guard.
#if defined(GLAD_GLES2_VERSION_2_0) || defined(GLAD_GLES2)
        if (!gladLoadGLES2(eglGetProcAddress))
        {
            std::fprintf(stderr, "GLAD failed to load GLES entry points.\n");
            out.Destroy();
            return false;
        }
#else
        std::fprintf(stderr,
                     "This build of GLAD does not include GLES loaders. Regenerate GLAD with GLES2/3 enabled.\n");
        out.Destroy();
        return false;
#endif
    }

    PrintEGLAndGLInfo(out.egl_display, out.egl_context);

    out.m_isInitialized = true;
    return true;
}

// Public init: OpenGL first, then fallback to OpenGL ES (3 -> 2)
inline bool InitEGL(int width = 256,
                    int height = 256,
                    int renderDevice = -1,
                    int glMajor = 4,
                    int glMinor = 5,
                    bool allowGLESFallback = true)
{
    EGLInternalData2 out;

    // 1) Try desktop OpenGL
    if (InitEGLTry(out, EGLInternalData2::API::OpenGL,
                   width, height, renderDevice, glMajor, glMinor, /*glesClientVersion*/ 0))
    {
        return true;
    }

    if (!allowGLESFallback)
        return false;

    std::fprintf(stderr, "Desktop OpenGL init failed — attempting OpenGL ES fallback...\n");

    // 2) Try GLES3
    if (InitEGLTry(out, EGLInternalData2::API::OpenGLES,
                   width, height, renderDevice, glMajor, glMinor, /*glesClientVersion*/ 3))
    {
        return true;
    }

    std::fprintf(stderr, "OpenGL ES 3.0 init failed — attempting OpenGL ES 2.0 fallback...\n");

    // 3) Try GLES2
    if (InitEGLTry(out, EGLInternalData2::API::OpenGLES,
                   width, height, renderDevice, glMajor, glMinor, /*glesClientVersion*/ 2))
    {
        return true;
    }

    return false;
}
