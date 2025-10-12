#include "backends/xrt/hls/depthrendererhls.h"
#include "backends/xrt/hls/texturehls.h"
#include "backends/xrt/hls/meshhls.h"
#include "backends/xrt/hls/rendererhls.h"
#include "core/types.h"
#include "core/camera.h"

extern "C"
{
    void TestRenderHLS(
        Scalar *out_texture_data,
        UInt out_texture_width,
        UInt out_texture_height,
        Scalar out_nodata_value,
        UInt out_lvl)
    {
#pragma HLS INTERFACE mode = m_axi port = out_texture_data bundle = gmem0 depth = 412800

        // data too large, has to be in ram
        TextureRAM<Scalar> out_texture(out_texture_width, out_texture_height, out_nodata_value, out_texture_data);
        // out_texture.fill(out_lvl, out_texture.nodata());
        //  for (int i = 0; i < out_texture_height * out_texture_width; i++)
        //{
        //      out_texture_data[i] = i + 100.0;
        //   }
        for (int y = 0; y < out_texture_height; y++)
        {
            for (int x = 0; x < out_texture_width; x++)
            {
                out_texture.set_texel_(x + 200.0, y, x, out_lvl);
            }
        }
    }
}