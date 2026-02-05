#pragma once

#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include "core/types.h"

template <typename T>
inline std::type_index GetTypeIndex()
{
    return std::type_index(typeid(T));
}

// Helper to make a dependent false for static_assert
template <class>
inline constexpr bool dependent_false_v = false;

template <typename T>
struct Channels
{
    static_assert(dependent_false_v<T>, "Channels<T>: unsupported type");
};

// Specializations:
template <>
struct Channels<unsigned char> : std::integral_constant<int, 1>
{
};
template <>
struct Channels<int> : std::integral_constant<int, 1>
{
};
template <>
struct Channels<float> : std::integral_constant<int, 1>
{
};

template <>
struct Channels<Vec2<float>> : std::integral_constant<int, 2>
{
};
template <>
struct Channels<Vec3<float>> : std::integral_constant<int, 3>
{
};
template <>
struct Channels<Vec3<int>> : std::integral_constant<int, 3>
{
};
template <>
struct Channels<Vec4<float>> : std::integral_constant<int, 4>
{
};

template <typename T>
constexpr int getChannels()
{
    return Channels<T>::value;
}
