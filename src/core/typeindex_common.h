#pragma once

#include <typeindex>
#include <typeinfo>

template <typename T>
inline std::type_index GetTypeIndex()
{
    return std::type_index(typeid(T));
}