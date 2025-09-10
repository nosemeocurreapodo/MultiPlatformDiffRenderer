#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
#include "core/types.h"
#include "core/camera.h"
#include "core/boundingbox.h"
#include "core/render_constants.h"
#include "backends/base/rendererbase.h"
#include "backends/xrt/hls/texturehls.h"
#include "backends/xrt/hls/bufferhls.h"
#include "backends/xrt/hls/meshhls.h"

// -----------------------------------------------------------------------------
// DepthRendererCPU
//   Example derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

class DepthRendererHLS
    : public DepthRendererBase<MeshHLS, TextureHLS<float>>
{
public:
    DepthRendererHLS() = default;
    ~DepthRendererHLS() = default;

    void Render(const MeshHLS &mesh,
                const SE3 &pose,
                const Camera &cam,
                int out_lvl,
                TextureHLS<float> &out_texture)
    {
        DepthRendererBase::Render(mesh, pose, cam, out_lvl, out_texture);
    }

private:
};

// -----------------------------------------------------------------------------
// ImageRendererCPU
//   Another example derived class that might output color
// -----------------------------------------------------------------------------

class ImageRendererHLS
    : public ImageRendererBase<MeshHLS, TextureHLS<float>, TextureHLS<float>>
{
public:
    ImageRendererCPU() = default;
    ~ImageRendererCPU() = default;

    void Render(const MeshHLS &mesh,
                const SE3 &pose,
                const Camera &cam,
                int in_lvl,
                int out_lvl,
                const TextureHLS<float> &in_texture,
                TextureHLS<float> &out_texture)
    {
        ImageRendererBase::Render(mesh, pose, cam, in_lvl, out_lvl, in_texture, out_texture);
    }

private:
};
