#pragma once

#include <typeindex>
#include <typeinfo>
#include "core/types.h"

template <typename T>
inline std::type_index GetTypeIndex()
{
    return std::type_index(typeid(T));
}

template <typename T>
inline int getChannels()
{
    if constexpr (std::is_same_v<T, unsigned char>)
    {
        return 1;
    }
    else if constexpr (std::is_same_v<T, int>)
    {
        return 1;
    }
    else if constexpr (std::is_same_v<T, float>)
    {
        return 1;
    }
    else if constexpr (std::is_same_v<T, Vec2<float>>)
    {
        return 2;
    }
    else if constexpr (std::is_same_v<T, Vec3<float>>)
    {
        return 3;
    }
    else if constexpr (std::is_same_v<T, Vec3<int>>)
    {
        return 3;
    }
    else if constexpr (std::is_same_v<T, Vec4<float>>)
    {
        return 4;
    }
    else
    {
        assert(false);
        return 0; // Unreachable
    }
}