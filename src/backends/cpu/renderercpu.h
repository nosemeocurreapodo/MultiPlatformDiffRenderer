#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
// #include "core/types.h"
#include "core/camera.h"
#include "core/boundingbox.h"
#include "core/render_constants.h"
#include "core/error_handling.h"
#include "backends/base/rendererbase.h"
#include "backends/cpu/devicecpu.h"
#include "backends/cpu/texturecpu.h"
#include "backends/cpu/buffercpu.h"
#include "backends/cpu/meshcpu.h"

class GouraudRendererCPU
    : public GouraudRendererBase<float, float, MeshCPU, TextureCPU>
{
public:
    GouraudRendererCPU() = default;
    ~GouraudRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const linalg::SE3<float> &pose,
                const Camera<float> &cam,
                const linalg::Vec3<float> &light_pos,
                const linalg::Vec3<float> &light_color,
                const linalg::Vec3<float> &ambient_reflectance,
                const linalg::Vec3<float> &diffuse_reflectance,
                const linalg::Vec3<float> &specular_reflectance,
                const float shininess,
                const linalg::Vec3<float> &ambient_light,
                unsigned int out_lvl,
                TextureCPU<linalg::Vec3<float>> &out_texture)
    {
        // Validate inputs
        ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        GouraudRendererBase::Render(mesh, pose, cam, light_pos, light_color, ambient_reflectance, diffuse_reflectance, specular_reflectance, shininess, ambient_light, out_lvl, out_texture);
    }

private:
};

// -----------------------------------------------------------------------------
// DepthRendererCPU
//   Example derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

class DepthRendererCPU
    : public DepthRendererBase<float, float, MeshCPU, TextureCPU>
{
public:
    DepthRendererCPU() = default;
    ~DepthRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const linalg::SE3<float> &pose,
                const Camera<float> &cam,
                int out_lvl,
                TextureCPU<float> &out_texture)
    {
        // Validate inputs
        ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        DepthRendererBase::Render(mesh, pose, cam, out_lvl, out_texture);
    }

private:
};

// -----------------------------------------------------------------------------
// ImageRendererCPU
//   Another example derived class that might output color
// -----------------------------------------------------------------------------

class ImageRendererCPU
    : public ImageRendererBase<float, float, MeshCPU, TextureCPU>
{
public:
    ImageRendererCPU() = default;
    ~ImageRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const linalg::SE3<float> &pose,
                const Camera<float> &cam,
                int in_lvl,
                int out_lvl,
                TextureCPU<float> &in_texture,
                TextureCPU<float> &out_texture)
    {
        ErrorHandling::ValidateTextureDimensions(out_texture.width(out_lvl), out_texture.height(out_lvl), out_lvl);
        ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        TextureCPU<float> depth_texture(out_texture.width(0), out_texture.height(0), -1.0f);

        ImageRendererBase::Render(mesh, pose, cam, in_lvl, out_lvl, depth_texture, in_texture, out_texture);
    }

private:
};

// -----------------------------------------------------------------------------
// ResidualRendererCPU
//   Example derived renderer that computes the residual between two frames.
// -----------------------------------------------------------------------------

class ResidualRendererCPU
    : public ResidualRendererBase<float, float, float, MeshCPU, TextureCPU>
{
public:
    ResidualRendererCPU() = default;
    ~ResidualRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const linalg::SE3<float> &pose,
                const Camera<float> &cam,
                int in_lvl,
                int out_lvl,
                TextureCPU<float> &kf_texture,
                TextureCPU<float> &f_texture,
                TextureCPU<float> &r_texture)
    {

        // ErrorHandling::ValidateTextureDimensions(r_texture.width(out_lvl), r_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        ResidualRendererBase::Render(mesh, pose, cam, in_lvl, out_lvl, kf_texture, f_texture, r_texture);
    }

private:
};

class L2RendererCPU
    : public L2RendererBase<float, float, float, MeshCPU, TextureCPU>
{
public:
    L2RendererCPU() = default;
    ~L2RendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const linalg::SE3<float> &pose,
                const Camera<float> &cam,
                int in_lvl,
                int out_lvl,
                TextureCPU<float> &kf_texture,
                TextureCPU<float> &f_texture,
                TextureCPU<float> &r_texture)
    {

        // ErrorHandling::ValidateTextureDimensions(r_texture.width(out_lvl), r_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        L2RendererBase::Render(mesh, pose, cam, in_lvl, out_lvl, kf_texture, f_texture, r_texture);
    }

private:
};

class DIDxyRendererCPU
    : public DIDxyRendererBase<float, float, float, MeshCPU, TextureCPU>
{
public:
    DIDxyRendererCPU() = default;
    ~DIDxyRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                int in_lvl,
                int out_lvl,
                TextureCPU<float> &in_texture,
                TextureCPU<linalg::Vec3<float>> &out_texture)
    {

        // ErrorHandling::ValidateTextureDimensions(r_texture.width(out_lvl), r_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        DIDxyRendererBase::Render(mesh, in_lvl, out_lvl, in_texture, out_texture);
    }

private:
};

class JPoseRendererCPU
    : public JPoseRendererBase<float, float, float, float, MeshCPU, TextureCPU>
{
public:
    JPoseRendererCPU() = default;
    ~JPoseRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const linalg::SE3<float> &pose,
                const Camera<float> &cam,
                int in_lvl,
                int out_lvl,
                TextureCPU<float> &kf_texture,
                TextureCPU<float> &f_texture,
                TextureCPU<linalg::Vec3<float>> &dfdxy_texture,
                TextureCPU<linalg::Vec3<float>> &jtra_texture,
                TextureCPU<linalg::Vec3<float>> &jrot_texture,
                TextureCPU<float> &r_texture)
    {

        // ErrorHandling::ValidateTextureDimensions(r_texture.width(out_lvl), r_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        JPoseRendererBase::Render(mesh, pose, cam, in_lvl, out_lvl, kf_texture, f_texture, dfdxy_texture, jtra_texture, jrot_texture, r_texture);
    }

private:
};

class JMapRendererCPU
    : public JMapRendererBase<float, float, float, float, float, MeshCPU, TextureCPU>
{
public:
    JMapRendererCPU() = default;
    ~JMapRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const linalg::SE3<float> &pose,
                const Camera<float> &cam,
                int in_lvl,
                int out_lvl,
                TextureCPU<float> &kf_texture,
                TextureCPU<float> &f_texture,
                TextureCPU<linalg::Vec3<float>> &dfdxy_texture,
                TextureCPU<linalg::Vec3<float>> &jmap_texture,
                TextureCPU<linalg::Vec3<float>> &pids_texture,
                TextureCPU<float> &r_texture)
    {

        // ErrorHandling::ValidateTextureDimensions(r_texture.width(out_lvl), r_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        JMapRendererBase::Render(mesh, pose, cam, in_lvl, out_lvl, kf_texture, f_texture, dfdxy_texture, jmap_texture, pids_texture, r_texture);
    }

private:
};

class DiffRendererCPU
    : public DiffRendererBase<float, float, float, float, float, MeshCPU, TextureCPU>
{
public:
    DiffRendererCPU() = default;
    ~DiffRendererCPU() = default;

    void Render(const MeshCPU &mesh,
                const linalg::SE3<float> &pose,
                const Camera<float> &cam,
                int in_lvl,
                int out_lvl,
                TextureCPU<float> &f_texture,
                TextureCPU<float> &image_texture,
                TextureCPU<float> &depth_texture,
                TextureCPU<linalg::Vec3<float>> &jtra_texture,
                TextureCPU<linalg::Vec3<float>> &jrot_texture,
                TextureCPU<linalg::Vec3<float>> &jmap_texture,
                TextureCPU<linalg::Vec3<float>> &pids_texture)
    {

        // ErrorHandling::ValidateTextureDimensions(r_texture.width(out_lvl), r_texture.height(out_lvl), out_lvl);
        // ErrorHandling::ValidateCameraParameters(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);

        DiffRendererBase::Render(mesh, pose, cam, in_lvl, out_lvl, f_texture, image_texture, depth_texture, jtra_texture, jrot_texture, jmap_texture, pids_texture);
    }

private:
};
