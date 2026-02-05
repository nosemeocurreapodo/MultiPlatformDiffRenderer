#pragma once

#include <type_traits>
#include <typeindex>
#include <stdexcept>
#include "core/types.h"
#include "core/typeindex_common.h"
#include "backends/gl/devicegl_glad.h"

// ---- Internal format trait ----
template <typename T>
struct GLInternalFormat
{
    static_assert(dependent_false_v<T>, "GLInternalFormat<T>: unsupported type");
};

// float
template <>
struct GLInternalFormat<float> : std::integral_constant<GLint, GL_R32F>
{
};
template <>
struct GLInternalFormat<Vec2<float>> : std::integral_constant<GLint, GL_RG32F>
{
};
template <>
struct GLInternalFormat<Vec3<float>> : std::integral_constant<GLint, GL_RGB32F>
{
};
template <>
struct GLInternalFormat<Vec4<float>> : std::integral_constant<GLint, GL_RGBA32F>
{
};

// int
template <>
struct GLInternalFormat<int> : std::integral_constant<GLint, GL_R32I>
{
};
template <>
struct GLInternalFormat<Vec2<int>> : std::integral_constant<GLint, GL_RG32I>
{
};
// NOTE: you had GL_RGB32F here (commented GL_RGB32I). Keep exactly what you had:
template <>
struct GLInternalFormat<Vec3<int>> : std::integral_constant<GLint, GL_RGB32F>
{
};
template <>
struct GLInternalFormat<Vec4<int>> : std::integral_constant<GLint, GL_RGBA32I>
{
};

// unsigned char
template <>
struct GLInternalFormat<unsigned char> : std::integral_constant<GLint, GL_R8>
{
};
// If you want Vec2/Vec3/Vec4<unsigned char> later, add:
// template <> struct GLInternalFormat<Vec2<unsigned char>> : std::integral_constant<GLint, GL_RG8> {};
// template <> struct GLInternalFormat<Vec3<unsigned char>> : std::integral_constant<GLint, GL_RGB8> {};
// template <> struct GLInternalFormat<Vec4<unsigned char>> : std::integral_constant<GLint, GL_RGBA8> {};

template <typename T>
constexpr GLint GetGLInternalFormat()
{
    return GLInternalFormat<T>::value;
}

// Optional runtime wrapper (keeps your old signature)
inline GLint GetGLInternalFormat(std::type_index ti)
{
    static const std::pair<std::type_index, GLint> table[] = {
        {GetTypeIndex<float>(), GetGLInternalFormat<float>()},
        {GetTypeIndex<Vec2<float>>(), GetGLInternalFormat<Vec2<float>>()},
        {GetTypeIndex<Vec3<float>>(), GetGLInternalFormat<Vec3<float>>()},
        {GetTypeIndex<Vec4<float>>(), GetGLInternalFormat<Vec4<float>>()},

        {GetTypeIndex<int>(), GetGLInternalFormat<int>()},
        {GetTypeIndex<Vec2<int>>(), GetGLInternalFormat<Vec2<int>>()},
        {GetTypeIndex<Vec3<int>>(), GetGLInternalFormat<Vec3<int>>()},
        {GetTypeIndex<Vec4<int>>(), GetGLInternalFormat<Vec4<int>>()},

        {GetTypeIndex<unsigned char>(), GetGLInternalFormat<unsigned char>()},
    };

    for (auto &[k, v] : table)
        if (k == ti)
            return v;

    throw std::invalid_argument("GetGLInternalFormat: unsupported type_index");
}

inline GLenum GetGLFormat(int channels)
{
    switch (channels)
    {
    case 1:
        return GL_RED;
    case 2:
        return GL_RG;
    case 3:
        return GL_RGB;
    case 4:
        return GL_RGBA;
    default:
        throw std::invalid_argument("GetGLFormat: channels must be 1..4");
    }
}

// ---- GL type trait ----
template <typename T>
struct GLType
{
    static_assert(dependent_false_v<T>, "GLType<T>: unsupported type");
};

// float family
template <>
struct GLType<float> : std::integral_constant<GLenum, GL_FLOAT>
{
};
template <>
struct GLType<Vec2<float>> : std::integral_constant<GLenum, GL_FLOAT>
{
};
template <>
struct GLType<Vec3<float>> : std::integral_constant<GLenum, GL_FLOAT>
{
};
template <>
struct GLType<Vec4<float>> : std::integral_constant<GLenum, GL_FLOAT>
{
};

// int family
template <>
struct GLType<int> : std::integral_constant<GLenum, GL_INT>
{
};
template <>
struct GLType<Vec2<int>> : std::integral_constant<GLenum, GL_INT>
{
};
// NOTE: you had GL_FLOAT here (commented GL_INT). Keep exactly what you had:
template <>
struct GLType<Vec3<int>> : std::integral_constant<GLenum, GL_FLOAT>
{
};
template <>
struct GLType<Vec4<int>> : std::integral_constant<GLenum, GL_INT>
{
};

// unsigned char
template <>
struct GLType<unsigned char> : std::integral_constant<GLenum, GL_UNSIGNED_BYTE>
{
};

template <typename T>
constexpr GLenum GetGLType()
{
    return GLType<T>::value;
}

// Optional runtime wrapper
inline GLenum GetGLType(std::type_index ti)
{
    static const std::pair<std::type_index, GLenum> table[] = {
        {GetTypeIndex<float>(), GetGLType<float>()},
        {GetTypeIndex<Vec2<float>>(), GetGLType<Vec2<float>>()},
        {GetTypeIndex<Vec3<float>>(), GetGLType<Vec3<float>>()},
        {GetTypeIndex<Vec4<float>>(), GetGLType<Vec4<float>>()},

        {GetTypeIndex<int>(), GetGLType<int>()},
        {GetTypeIndex<Vec2<int>>(), GetGLType<Vec2<int>>()},
        {GetTypeIndex<Vec3<int>>(), GetGLType<Vec3<int>>()},
        {GetTypeIndex<Vec4<int>>(), GetGLType<Vec4<int>>()},

        {GetTypeIndex<unsigned char>(), GetGLType<unsigned char>()},
    };

    for (auto &[k, v] : table)
        if (k == ti)
            return v;

    throw std::invalid_argument("GetGLType: unsupported type_index");
}
