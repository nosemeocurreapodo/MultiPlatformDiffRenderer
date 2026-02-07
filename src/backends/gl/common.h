#pragma once

#include "backends/gl/devicegl_glad.h"

void create_framebuffer(GLuint &fbo, GLuint &rbo)
{
#if defined(GL_VERSION_4_5)
    if (GLAD_GL_VERSION_4_5)
    {
        glCreateFramebuffers(1, &fbo);
    }
    else
#endif
    {
        glGenFramebuffers(1, &fbo);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1280, 1280); // use a single renderbuffer object for both a depth AND stencil buffer.

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void check_framebuffer()
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        throw RendererExceptions::OpenGLException("framebuffer setup", status);
    }
}

// void save_state()
//{
//     glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo_);
//     glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg_);
//     glGetIntegerv(GL_VIEWPORT, prevViewport_);
// }

// void restore_state()
//{
//     glUseProgram(prevProg_);
//     glBindFramebuffer(GL_FRAMEBUFFER, prevFbo_);
//     glViewport(prevViewport_[0], prevViewport_[1], prevViewport_[2], prevViewport_[3]);
// }

static const char *shader_type_name(GLenum type)
{
    switch (type)
    {
    case GL_VERTEX_SHADER:
        return "Vertex";
    case GL_FRAGMENT_SHADER:
        return "Fragment";
    case GL_GEOMETRY_SHADER:
        return "Geometry";
    case GL_COMPUTE_SHADER:
        return "Compute";
    default:
        return "Unknown";
    }
}

GLuint compile_shader(GLenum type, const char *src, const char *common = nullptr)
{
    if (!src || !*src)
        throw RendererExceptions::OpenGLException("Shader compilation (empty source)", GL_FALSE);

    if (!common)
        common = "";

    GLuint id = glCreateShader(type);
    if (id == 0)
        throw RendererExceptions::OpenGLException("glCreateShader failed", GL_FALSE);

    // If common is empty, compile as-is.
    if (*common == '\0')
    {
        glShaderSource(id, 1, &src, nullptr);
        glCompileShader(id);
    }
    else
    {
        // Find end of the #version line in src, and insert common right after it.
        const char *p = src;

        // Skip UTF-8 BOM if present
        if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
            p += 3;

        // Skip leading whitespace/newlines (GLSL allows whitespace before #version)
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            ++p;

        const char *insert_pos = src; // fallback (if #version not found)
        if (std::strncmp(p, "#version", 8) == 0)
        {
            const char *nl = std::strchr(p, '\n');
            insert_pos = nl ? (nl + 1) : (src + std::strlen(src)); // after #version line
        }

        // Compute original line number at insert_pos so we can restore it after common.
        // (#line sets the line number for the *next* line.)
        int rest_line = 1;
        for (const char *t = src; t < insert_pos; ++t)
            if (*t == '\n')
                ++rest_line;

        // Use #line to make error locations nicer:
        // - common reports as "file 1"
        // - original src reports as "file 0" and with its original line numbers
        std::string common_prolog = std::string("\n#line 1 1\n");
        std::string main_epilog = std::string("\n#line ") + std::to_string(rest_line) + " 0\n";

        const char *parts[] = {
            src, // prefix (includes #version line)
            common_prolog.c_str(),
            common,
            main_epilog.c_str(),
            insert_pos // rest of src
        };

        GLint lens[] = {
            (GLint)(insert_pos - src), // prefix length (not null-terminated)
            -1,
            -1,
            -1,
            -1};

        glShaderSource(id, 5, parts, lens);
        glCompileShader(id);
    }

    GLint ok = GL_FALSE;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);

    if (!ok)
    {
        GLint log_len = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_len);

        std::string log;
        log.resize((log_len > 1) ? (size_t)log_len : 1);

        GLsizei written = 0;
        glGetShaderInfoLog(id, (GLsizei)log.size(), &written, log.data());
        if (written > 0)
            log.resize((size_t)written);

        std::string where = std::string(shader_type_name(type)) + " shader compilation";
        glDeleteShader(id);

        std::cerr << where << " failed:\n"
                  << log << "\n";
        throw RendererExceptions::OpenGLException(where, ok);
    }

    return id;
}

GLuint create_program(const char *vs, const char *gs, const char *fs, const char *common = nullptr)
{
    GLuint vsId, gsId, fsId;
    vsId = compile_shader(GL_VERTEX_SHADER, vs, common);
    if (gs)
        gsId = compile_shader(GL_GEOMETRY_SHADER, gs, common);
    fsId = compile_shader(GL_FRAGMENT_SHADER, fs, common);

    GLuint program = glCreateProgram();
    glAttachShader(program, vsId);
    if(gs)
    glAttachShader(program, gsId);
    glAttachShader(program, fsId);
    glLinkProgram(program);
    glDeleteShader(vsId);
    if(gs)
    glDeleteShader(gsId);
    glDeleteShader(fsId);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        glDeleteProgram(program);
        program = 0;
        throw RendererExceptions::OpenGLException("shader program linking", 0);
    }
    return program;
}

GLuint create_program(const char *cs, const char *common = nullptr)
{
    GLuint csId = compile_shader(GL_COMPUTE_SHADER, cs, common);

    GLuint program = glCreateProgram();
    glAttachShader(program, csId);
    glLinkProgram(program);
    glDeleteShader(csId);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        glDeleteProgram(program);
        program = 0;
        throw RendererExceptions::OpenGLException("shader program linking", 0);
    }
    return program;
}