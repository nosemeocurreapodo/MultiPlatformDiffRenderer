#pragma once

#include "glad/egl.h"
#ifdef COMPILE_GL
#include "glad/gl.h"
#endif
#ifdef COMPILE_GLES2
#include "glad/gles2.h"
#endif

static void PrintEGLAndGLInfo(EGLDisplay dpy, EGLContext ctx)
{
    // --- EGL side ---
    EGLint eMaj = 0, eMin = 0;
    eglInitialize(dpy, &eMaj, &eMin);
    const char *egl_vendor = eglQueryString(dpy, EGL_VENDOR);
    const char *egl_verstr = eglQueryString(dpy, EGL_VERSION);
    const char *egl_apis = eglQueryString(dpy, EGL_CLIENT_APIS);
    const char *egl_exts = eglQueryString(dpy, EGL_EXTENSIONS);

    printf("[EGL] %d.%d  vendor=%s  version=\"%s\"\n", eMaj, eMin,
           egl_vendor ? egl_vendor : "?", egl_verstr ? egl_verstr : "?");
    printf("[EGL] client APIs: %s\n", egl_apis ? egl_apis : "?");
    // printf("[EGL] extensions: %s\n", egl_exts ? egl_exts : "?");

    // What client did we ask for?
    EGLint clientVer = 0;
    if (ctx)
        eglQueryContext(dpy, ctx, EGL_CONTEXT_CLIENT_VERSION, &clientVer);
    printf("[EGL] requested OpenGL ES version: %d (0 means desktop GL or unknown)\n", clientVer);

    // --- GL/ES side (must have a current context) ---
    const GLubyte *gl_ver = glGetString(GL_VERSION);
    const GLubyte *gl_vendor = glGetString(GL_VENDOR);
    const GLubyte *gl_renderer = glGetString(GL_RENDERER);
    const GLubyte *gl_shading = glGetString(GL_SHADING_LANGUAGE_VERSION);

    printf("[GL]  version: %s\n", gl_ver ? (const char *)gl_ver : "?");
    printf("[GL]  vendor:  %s\n", gl_vendor ? (const char *)gl_vendor : "?");
    printf("[GL]  renderer:%s\n", gl_renderer ? (const char *)gl_renderer : "?");
    printf("[GL]  GLSL:    %s\n", gl_shading ? (const char *)gl_shading : "?");

    // If API supports it, ask for numeric major/minor:
#if defined(GL_MAJOR_VERSION) // defined in headers; availability still depends on runtime
    GLint maj = 0, min = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &maj);
    glGetIntegerv(GL_MINOR_VERSION, &min);
    if (glGetError() == GL_NO_ERROR && maj > 0)
    {
        printf("[GL]  numeric version: %d.%d\n", maj, min);
    }
    else
    {
        printf("[GL]  numeric version: (not available; likely ES 2.0)\n");
    }
#endif

    // glad feature flags are handy too:
#if defined(GLAD_GL_ES_VERSION_3_2)
    if (GLAD_GL_ES_VERSION_3_2)
        puts("[GLAD] ES 3.2 context");
    else if (GLAD_GL_ES_VERSION_3_1)
        puts("[GLAD] ES 3.1 context");
    else if (GLAD_GL_ES_VERSION_3_0)
        puts("[GLAD] ES 3.0 context");
    else if (GLAD_GL_ES_VERSION_2_0)
        puts("[GLAD] ES 2.0 context");
#endif
#if defined(GLAD_GL_VERSION_4_6)
    if (GLAD_GL_VERSION_4_6)
        puts("[GLAD] Desktop GL 4.6 context");
    // ... etc
#endif
}