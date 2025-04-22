#pragma once

template <typename Type>
class Texture
{
public:
    virtual void FromCPU(Type *data) = 0;
    virtual void ToCPU(Type *data) = 0;

private:
};
