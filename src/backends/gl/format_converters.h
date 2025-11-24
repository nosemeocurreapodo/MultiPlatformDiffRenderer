#pragma once

#include <map>
#include "core/types.h"
#include "core/typeindex_common.h"
#include "backends/gl/devicegl_glad.h"

inline GLint GetGLInternalFormat(std::type_index tindex)
{
    // static std::map<std::pair<std::type_index, int>, GLint> internal_format_map;
    static std::map<std::type_index, GLint> internal_format_map;
    // internal_format_map[GetTypeIndex<float>()] = GL_R32F;
    // internal_format_map[GetTypeIndex<ImageType>()] = GL_R32F;
    internal_format_map[GetTypeIndex<float>()] = GL_R32F;
    internal_format_map[GetTypeIndex<Vec2<float>>()] = GL_RG32F;
    internal_format_map[GetTypeIndex<Vec3<float>>()] = GL_RGB32F;
    internal_format_map[GetTypeIndex<Vec4<float>>()] = GL_RGBA32F;

    internal_format_map[GetTypeIndex<int>()] = GL_R32I;
    internal_format_map[GetTypeIndex<Vec2<int>>()] = GL_RG32I;
    internal_format_map[GetTypeIndex<Vec3<int>>()] = GL_RGB32F; // GL_RGB32I;
    internal_format_map[GetTypeIndex<Vec4<int>>()] = GL_RGBA32I;

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
    type_map[GetTypeIndex<float>()] = GL_FLOAT;
    type_map[GetTypeIndex<Vec2<float>>()] = GL_FLOAT;
    type_map[GetTypeIndex<Vec3<float>>()] = GL_FLOAT;
    type_map[GetTypeIndex<Vec4<float>>()] = GL_FLOAT;
    type_map[GetTypeIndex<int>()] = GL_INT;
    type_map[GetTypeIndex<Vec2<int>>()] = GL_INT;
    type_map[GetTypeIndex<Vec3<int>>()] = GL_FLOAT; // GL_INT;
    type_map[GetTypeIndex<Vec4<int>>()] = GL_INT;
    type_map[GetTypeIndex<unsigned char>()] = GL_UNSIGNED_BYTE;

    return type_map[tindex];
}
