#pragma once

#include "common/camera.h"
#include "mesh.h"
#include "buffer.h"
#include "texture.h"

template <typename InTexType, typename OutTexType>
class Renderer
{
public:
    //Renderer() = default;
    //virtual ~Renderer() = default;

    virtual void Render(const Mesh &mesh,
                        const SE3 &pose,
                        const CameraType &cam,
                        const Texture<InTexType> &in_texture,
                        Texture<OutTexType> &out_texture,
                        int lvl) = 0;
};