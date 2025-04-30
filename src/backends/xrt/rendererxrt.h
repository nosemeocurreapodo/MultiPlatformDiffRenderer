#pragma once

#include "devicexrt.h"
#include "bufferxrt.h"
#include "texturexrt.h"
#include "meshxrt.h"

template <typename InTexType, typename OutTexType>
class BaseRendererXRT
{
public:
    BaseRendererXRT()
    {
    }

    void Render(const MeshXRT &mesh, const SE3 pose, const CameraType cam, const TextureXRT<InTexType> &texture, TextureXRT<OutTexType> &buffer, int lvl)
    {
        kernel(vertices_bo,
               vertices.size(),
               texcoords_bo,
               texcoords.size(),
               weights_bo,
               weights.size(),
               tris_bo,
               tris_f.size(),
               &pose,
               &cam,
               in_texture_bo,
               w,
               h,
               1,
               out_texture_bo,
               w,
               h,
               1);
    }

protected:
    xrt::bo vertices_bo;
    xrt::bo texcoords_bo;
    xrt::bo weights_bo;
    xrt::bo tris_bo;

    xrt::bo in_texture_bo;
    xrt::bo out_texture_bo;

    xrt::kernel kernel;
};

class DepthRendererXRT : public BaseRendererXRT<float /*InTexType*/, float /*OutTexType*/>
{
public:
    DepthRendererXRT() : BaseRendererXRT()
    {
        kernel = xrt::kernel(device_xrt, uuid_xrt, "DepthRenderFPGA");

        vertices_bo = xrt::bo(device_xrt, 1024, kernel.group_id(0));
        texcoords_bo = xrt::bo(device_xrt, 1024, kernel.group_id(2));
        weights_bo = xrt::bo(device_xrt, 1024, kernel.group_id(4));
        tris_bo = xrt::bo(device_xrt, 1024, kernel.group_id(6));

        in_texture_bo = xrt::bo(device_xrt, 1024, kernel.group_id(10));
        out_texture_bo = xrt::bo(device_xrt, 1024, kernel.group_id(14));
    }

private:
};