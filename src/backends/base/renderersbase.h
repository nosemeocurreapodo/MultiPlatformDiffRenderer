#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
// #include "linalg/linalg.h"
#include "backends/base/texturebase.h"
#include "core/render_constants.h"
#include "core/types.h"

#ifdef USE_VITIS
#include "backends/xrt/hls/math_common.h"
#else
#include "backends/cpu/math_common.h"
#endif

template <template <class> class TextureViewWrite>
class DeferredRendererBase
{
public:
    DeferredRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        const IntType not_used;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> gbuf_fpos;
        TextureViewWrite<Vec3<float>> gbuf_kfpos;
        TextureViewWrite<Vec3<float>> gbuf_bcid;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
    };

    struct Varyings
    {
        Vec3<RealType> kf_ver;
        Vec3<RealType> f_ver;
        Vec3<RealType> baricentric;
        IntType tri_id;
    };

    struct Fragment
    {
        Vec3<RealType> fpos;
        Vec3<RealType> kfpos;
        Vec3<RealType> bcid;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        Vec3<float> fpos_nodata(textures.gbuf_fpos.nodata());
        Vec3<float> kfpos_nodata(textures.gbuf_kfpos.nodata());
        Vec3<float> bcid_nodata(textures.gbuf_bcid.nodata());

        return Fragment{Vec3<RealType>(fpos_nodata(0), fpos_nodata(1), fpos_nodata(2)),
                        Vec3<RealType>(kfpos_nodata(0), kfpos_nodata(1), kfpos_nodata(2)),
                        Vec3<RealType>(bcid_nodata(0), bcid_nodata(1), bcid_nodata(2))};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0, const Varyings &varying_px1, const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.baricentric = Vec3<RealType>(w0,
                                                   w1,
                                                   w2);
        var_over_w_px.tri_id = triangle_id;

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        Vec4<RealType> kf_ver = Vec4<RealType>(vertexdata.vertex(0),
                                               vertexdata.vertex(1),
                                               vertexdata.vertex(2),
                                               RealType(1));
        Vec4<RealType> f_ver = uniforms.pose_matrix * kf_ver;
        gl_Position = uniforms.view_matrix * f_ver;

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ver = Vec3<RealType>(kf_ver(0), kf_ver(1), kf_ver(2));
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;
        Vec3<RealType> bc = in_varying.baricentric;
        IntType tri_id = in_varying.tri_id;

        fragment.fpos = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        fragment.kfpos = Vec3<RealType>(kf_ver(0), kf_ver(1), kf_ver(2));
        fragment.bcid = Vec3<RealType>(bc(0), bc(1), tri_id);
    }

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
        Vec3<RealType> fpos = fragment.fpos;
        Vec3<RealType> kfpos = fragment.kfpos;
        Vec3<RealType> bcid = fragment.bcid;

        Vec3<float> fpos_out(fpos(0), fpos(1), fpos(2));
        Vec3<float> kfpos_out(kfpos(0), kfpos(1), kfpos(2));
        Vec3<float> bcid_out(bcid(0), bcid(1), bcid(2));

        textures.gbuf_fpos(y, x) = fpos_out;
        textures.gbuf_kfpos(y, x) = kfpos_out;
        textures.gbuf_bcid(y, x) = bcid_out;
    }
};

// -----------------------------------------------------------------------------
// DepthRenderer
//   Evout[0].screen(0)mple derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

template <template <class> class TextureView>
class DepthRendererBase
{
public:
    DepthRendererBase() = delete;
    //~DepthRendererBase() = delete;

    struct InTextures
    {
        const RealType unused;
    };

    struct OutTextures
    {
        TextureView<float> out_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
        // Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        Mat4<RealType> pose_matrix;
        Mat4<RealType> view_matrix;
    };

    struct Varyings
    {
        RealType depth;
    };

    struct Fragment
    {
        RealType depth;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS INLINE

        return Fragment{RealType(textures.out_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
#pragma HLS INLINE

        VertexData vertexdata;
        IntType base = vertexid * 3;
        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0,
                                         const Varyings &varying_px1,
                                         const Varyings &varying_px2)
    {
#pragma HLS INLINE

        Varyings var_over_w_px;
        var_over_w_px.depth =
            (w0 * varying_px0.depth +
             w1 * varying_px1.depth +
             w2 * varying_px2.depth);
        return var_over_w_px;
    }

    template <class DVaryings>
    static Varyings varyings_from_deferred(DVaryings &dvaryings)
    {
        Varyings varyings;
        varyings.depth = dvaryings.fpos(2);
        return varyings;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
#pragma HLS INLINE

        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));

        //   std::cout << "calling vertex shader " << std::endl;
        gl_Position = uniforms.view_matrix * f_ver;
        outVarying.depth = f_ver(2);
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
#pragma HLS INLINE

        RealType depth = in_varying.depth;
        // RealType depth = gl_FragCoord(2);

        fragment.depth = depth;
    }

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
        RealType depth = fragment.depth;
        textures.out_texture(y, x) = depth;
    }
};

// -----------------------------------------------------------------------------
// ImageRendererBase
//   Another evout[0].screen(0)mple derived class that might output color
// -----------------------------------------------------------------------------

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class ImageRendererBase
{
public:
    ImageRendererBase() = delete;
    //~ImageRendererBase() = default;

    struct InTextures
    {
        TextureViewRead<ImageType> in_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<ImageType> out_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        Mat4<RealType> pose_matrix;
        Mat4<RealType> view_matrix;
        PinholeCamera<RealType> camera;
        Vec2<RealType> exposure;
        IntType in_lvl;
        IntType out_lvl;
    };

    struct Varyings
    {
        Vec3<RealType> kf_ver;
    };

    struct Fragment
    {
        RealType color;
    };

    struct Samples
    {
        RealType sample;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
        // #pragma HLS inline

        return Fragment{RealType(textures.out_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const unsigned int vertexid)
    {
        // #pragma HLS inline

        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0, const Varyings &varying_px1, const Varyings &varying_px2)
    {
        // #pragma HLS inline

        Varyings var_over_w_px;
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        return var_over_w_px;
    }

    template <class DVaryings>
    static Varyings varyings_from_deferred(DVaryings &dvaryings)
    {
        Varyings varyings;
        varyings.kf_ver = dvaryings.kfpos;
        return varyings;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        // #pragma HLS inline

        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));

        gl_Position = uniforms.view_matrix * f_ver;
        outVarying.kf_ver = vertexdata.vertex;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
#pragma HLS inline

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(in_varying.kf_ver);

        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
        {
            return;
        }

        Samples samples;
        samples.sample = sample<RealType, TextureViewRead<ImageType>>(intextures.in_texture,
                                                                      texcoord(1), texcoord(0));

        fragment_shader(uniforms, in_varying, samples, fragment);
    }

    static void fragment_shader(const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const Samples &samples,
                                Fragment &fragment)
    {
#pragma HLS inline

        RealType pix = samples.sample;

        // AddressMode::Clamp,
        // FilterMode::Nearest);
        pix = apply_exposure(pix, uniforms.exposure);
        fragment.color = pix;
    }

    static void sync_intextures(const InTextures &textures, const BoundingBox<IntType> &tex_bb, const Uniforms uniforms, const Varyings varyings_buffer[], Samples sample_buffer[])
    {
        // #pragma HLS INLINE

    depthrendererbase_sync_outtexture_y_loop:
        for (IntType iy = 0; iy < tex_bb.height_; iy++)
        {
#pragma HLS loop_tripcount min = MAX_TILE_HEIGHT max = MAX_TILE_HEIGHT avg = MAX_TILE_HEIGHT

        depthrendererbase_sync_outtexture_x_loop:
            for (IntType ix = 0; ix < tex_bb.width_; ix++)
            {
#pragma HLS loop_tripcount min = MAX_TILE_WIDTH max = MAX_TILE_WIDTH avg = MAX_TILE_WIDTH

                IntType x = ix + tex_bb.min_x_;
                IntType y = iy + tex_bb.min_y_;
                IntType address = iy * tex_bb.width_ + ix;

                // ImageType color = fragment_buffer[address].color;
                // textures.out_texture(y, x) = color;

                Vec2<RealType> texcoord = varyings_buffer[address].texcoord;

                RealType pix = sample<RealType, TextureViewRead<ImageType>>(textures.in_texture,
                                                                            texcoord(1), texcoord(0));
                sample_buffer[address].sample = pix;
            }
        }
    }

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
        ImageType color = fragment.color;
        textures.out_texture(y, x) = color;
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class DIDxyRendererBase
{
public:
    DIDxyRendererBase() = delete;
    //~DIDxyRendererBase() = default;

    struct InTextures
    {
        TextureViewRead<ImageType> in_texture;
    };
    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> out_texture;
    };

    struct VertexData
    {
        Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        IntType in_lvl;
        IntType out_lvl;
    };

    struct Varyings
    {
        Vec2<RealType> texcoord;
    };

    struct Fragment
    {
        Vec3<RealType> didxy;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        return Fragment{Vec3<RealType>(textures.out_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 2;

        vertexdata.texcoord(0) = vertex_buffer[base + 0];
        vertexdata.texcoord(1) = vertex_buffer[base + 1];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0, const Varyings &varying_px1, const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord +
             w1 * varying_px1.texcoord +
             w2 * varying_px2.texcoord);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        // gl_Position = (view_matrix * pose_matrix) * Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = Vec4<RealType>(RealType(2) * vertexdata.texcoord(0) - RealType(1), RealType(2) * vertexdata.texcoord(1) - RealType(1), RealType(0), RealType(1));
        outVarying.texcoord = vertexdata.texcoord;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        if (in_varying.texcoord(0) < RealType(0) || in_varying.texcoord(0) > RealType(1) ||
            in_varying.texcoord(1) < RealType(0) || in_varying.texcoord(1) > RealType(1))
            return;

        IntType height = intextures.in_texture.height();
        IntType width = intextures.in_texture.width();
        ImageType nodata = intextures.in_texture.nodata();

        // if(in_varying.texcoord(0) > 0.5)
        //     return;

        // IntType x = IntType(in_varying.texcoord(0) * RealType(width - 1));
        // IntType y = IntType(in_varying.texcoord(1) * RealType(height - 1));
        IntType x = IntType(gl_FragCoord(0));
        IntType y = IntType(gl_FragCoord(1));
        IntType x_p = x + 1;
        IntType x_pp = x + 2;
        IntType x_m = x - 1;
        IntType x_mm = x - 2;
        IntType y_p = y + 1;
        IntType y_pp = y + 2;
        IntType y_m = y - 1;
        IntType y_mm = y - 2;

        if (x_p >= width || x_m < 0 || y_p >= height || y_m < 0) // ||
                                                                 //   x_pp >= width || x_mm < 0 || y_pp >= height || y_mm < 0)
        {
            //  No need to explicitly set to nodata, it is already in the background color
            return;
        }

        ImageType f = intextures.in_texture(y, x);
        ImageType f_y_p = intextures.in_texture(y_p, x);
        ImageType f_y_m = intextures.in_texture(y_m, x);
        ImageType f_x_p = intextures.in_texture(y, x_p);
        ImageType f_x_m = intextures.in_texture(y, x_m);
        // ImageType f_y_pp = intextures.in_texture(y_pp, x);
        // ImageType f_y_mm = intextures.in_texture(y_mm, x);
        // ImageType f_x_pp = intextures.in_texture(y, x_pp);
        // ImageType f_x_mm = intextures.in_texture(y, x_mm);

        // if (f_x_p == nodata || f_x_m == nodata ||
        //     f_y_p == nodata || f_y_m == nodata || f == nodata)
        //{
        //   No need to explicitly set to nodata, it is already in the background color
        //    return;
        //}

        Vec3<RealType> out_fragment;
        // out_fragment(0) = (-RealType(f_x_pp) + RealType(8) * RealType(f_x_p) - RealType(8) * RealType(f_x_m) + RealType(f_x_mm)) / RealType(12);
        // out_fragment(1) = (-RealType(f_y_pp) + RealType(8) * RealType(f_y_p) - RealType(8) * RealType(f_y_m) + RealType(f_y_mm)) / RealType(12);
        //   out_fragment(2) = RealType(f);
        out_fragment(0) = (RealType(f_x_p) - RealType(f_x_m)) / RealType(2);
        out_fragment(1) = (RealType(f_y_p) - RealType(f_y_m)) / RealType(2);
        out_fragment(2) = RealType(0); // f; // save the projected frame for later processing

        // if(out_fragment.norm() < RealType(50))
        //     return;

        fragment.didxy = out_fragment;
    }

    /*
    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        if (in_varying.texcoord(0) < RealType(0) || in_varying.texcoord(0) > RealType(1) ||
            in_varying.texcoord(1) < RealType(0) || in_varying.texcoord(1) > RealType(1))
            return;

        IntType height = intextures.in_texture.height();
        IntType width = intextures.in_texture.width();
        ImageType nodata = intextures.in_texture.nodata();

        // IntType x = IntType(in_varying.texcoord(0) * RealType(width - 1));
        // IntType y = IntType(in_varying.texcoord(1) * RealType(height - 1));
        IntType x = IntType(gl_FragCoord(0));
        IntType y = IntType(gl_FragCoord(1));

        if (x <= 0 || x >= width - 1 || y <= 0 || y >= height - 1)
            return;

        RealType sobel_x[3][3];
        RealType sobel_y[3][3];

        sobel_x[0][0] = RealType(-1);
        sobel_x[0][1] = RealType(0);
        sobel_x[0][2] = RealType(1);
        sobel_x[1][0] = RealType(-2);
        sobel_x[1][1] = RealType(0);
        sobel_x[1][2] = RealType(2);
        sobel_x[2][0] = RealType(-1);
        sobel_x[2][1] = RealType(0);
        sobel_x[2][2] = RealType(1);

        sobel_y[0][0] = RealType(-1);
        sobel_y[1][0] = RealType(0);
        sobel_y[2][0] = RealType(1);
        sobel_y[0][1] = RealType(-2);
        sobel_y[1][1] = RealType(0);
        sobel_y[2][1] = RealType(2);
        sobel_y[0][2] = RealType(-1);
        sobel_y[1][2] = RealType(0);
        sobel_y[2][2] = RealType(1);

        Vec3<RealType> out_fragment(0, 0, 0);
        for (int j = 0; j < 3; j++)
            for (int i = 0; i < 3; i++)
            {
                ImageType val = intextures.in_texture(y + j - 1, x + i - 1);
                out_fragment(0) += sobel_x[j][i] * RealType(val);
                out_fragment(1) += sobel_y[j][i] * RealType(val);
            }

        fragment.didxy = out_fragment;
    }
    */
    /*
    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        // if (in_varying.texcoord(0) > 0.5)
        //   return;

        IntType height = intextures.in_texture.height(uniforms.in_lvl);
        IntType width = intextures.in_texture.width(uniforms.in_lvl);
        ImageType nodata = intextures.in_texture.nodata();

        //IntType x = IntType(in_varying.texcoord(0) * RealType(width - 1));
        //IntType y = IntType(in_varying.texcoord(1) * RealType(height - 1));
        IntType x = IntType(gl_FragCoord(0));
        IntType y = IntType(gl_FragCoord(1));

        if (x <= 1 || x >= width - 2 || y <= 1 || y >= height - 2)
            return;

        RealType scharr5x5[5][5];

        scharr5x5[0][0] = RealType(-1);
        scharr5x5[0][1] = RealType(-1);
        scharr5x5[0][2] = RealType(0);
        scharr5x5[0][3] = RealType(1);
        scharr5x5[0][4] = RealType(1);

        scharr5x5[1][0] = RealType(-2);
        scharr5x5[1][1] = RealType(-2);
        scharr5x5[1][2] = RealType(0);
        scharr5x5[1][3] = RealType(2);
        scharr5x5[1][4] = RealType(2);

        scharr5x5[2][0] = RealType(-3);
        scharr5x5[2][1] = RealType(-6);
        scharr5x5[2][2] = RealType(0);
        scharr5x5[2][3] = RealType(6);
        scharr5x5[2][4] = RealType(3);

        scharr5x5[3][0] = RealType(-2);
        scharr5x5[3][1] = RealType(-2);
        scharr5x5[3][2] = RealType(0);
        scharr5x5[3][3] = RealType(2);
        scharr5x5[3][4] = RealType(2);

        scharr5x5[4][0] = RealType(-1);
        scharr5x5[4][1] = RealType(-1);
        scharr5x5[4][2] = RealType(0);
        scharr5x5[4][3] = RealType(1);
        scharr5x5[4][4] = RealType(1);

        Vec3<RealType> out_fragment(0, 0, 0);
        for (int j = 0; j < 5; j++)
            for (int i = 0; i < 5; i++)
            {
                ImageType val = intextures.in_texture.texel_(y + j - 2, x + i - 2, uniforms.in_lvl);
                out_fragment(0) += scharr5x5[j][i] * RealType(val);
                out_fragment(1) += scharr5x5[i][j] * RealType(val);
            }

        fragment.didxy = out_fragment;
    }
    */

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
        Vec3<RealType> didxy = fragment.didxy;
        textures.out_texture(y, x) = didxy;
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class DIDexpRendererBase
{
public:
    DIDexpRendererBase() = delete;
    //~DIDxyRendererBase() = default;

    struct InTextures
    {
        const TextureViewRead<ImageType> in_texture;
    };
    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> out_texture;
    };

    struct VertexData
    {
        Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        IntType in_lvl;
        IntType out_lvl;
        Vec2<RealType> exposure;
    };

    struct Varyings
    {
        Vec2<RealType> texcoord;
    };

    struct Fragment
    {
        Vec3<RealType> didexp;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        return Fragment{Vec3<RealType>(textures.out_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 2;

        vertexdata.texcoord(0) = vertex_buffer[base + 0];
        vertexdata.texcoord(1) = vertex_buffer[base + 1];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0, const Varyings &varying_px1, const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord +
             w1 * varying_px1.texcoord +
             w2 * varying_px2.texcoord);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        // gl_Position = (view_matrix * pose_matrix) * Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = Vec4<RealType>(RealType(2) * vertexdata.texcoord(0) - RealType(1), RealType(2) * vertexdata.texcoord(1) - RealType(1), RealType(0), RealType(1));
        outVarying.texcoord = vertexdata.texcoord;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        if (in_varying.texcoord(0) < RealType(0) || in_varying.texcoord(0) > RealType(1) ||
            in_varying.texcoord(1) < RealType(0) || in_varying.texcoord(1) > RealType(1))
            return;

        IntType height = intextures.in_texture.height();
        IntType width = intextures.in_texture.width();
        ImageType nodata = intextures.in_texture.nodata();

        IntType x = IntType(in_varying.texcoord(0) * RealType(width - 1));
        IntType y = IntType(in_varying.texcoord(1) * RealType(height - 1));

        ImageType f = intextures.in_texture(y, x);

        if (f == intextures.in_texture.nodata())
            return;

        Vec3<RealType> d = d_f_exp_d_exp(RealType(f), uniforms.exposure);

        Vec3<RealType> out_fragment;
        out_fragment(0) = d(0);        // b = f*exp(param(0)) + param(1)
        out_fragment(1) = d(1);        // dbdexp = f*exp(param(0)), 1.0
        out_fragment(2) = RealType(0); // f; // save the projected frame for later processing

        fragment.didexp = out_fragment;
    }

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
        Vec3<RealType> didexp = fragment.didexp;
        textures.out_texture(y, x) = didexp;
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class JPoseExpRendererBase
{
public:
    JPoseExpRendererBase() = delete;
    //~JPoseRendererBase() = default;

    struct InTextures
    {
        const TextureViewRead<ImageType> kf_texture;
        const TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jtra_texture;
        TextureViewWrite<Vec3<float>> jrot_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        Vec2<RealType> exposure;
        PinholeCamera<RealType> camera;
        IntType out_width;
        IntType out_height;
    };

    struct Varyings
    {
        Vec3<RealType> f_ver;
        Vec3<RealType> kf_ver;
    };

    struct Fragment
    {
        Vec3<RealType> jtra;
        Vec3<RealType> jrot;
        Vec3<RealType> jexp;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        return Fragment{Vec3<RealType>(textures.jtra_texture.nodata()),
                        Vec3<RealType>(textures.jrot_texture.nodata()),
                        Vec3<RealType>(textures.jexp_texture.nodata()),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0, const Varyings &varying_px1, const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        return var_over_w_px;
    }
    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));
        gl_Position = uniforms.view_matrix * f_ver;

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ver = vertexdata.vertex;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(kf_ver);

        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
            return;

        RealType kf = sample<RealType, TextureViewRead<ImageType>>(intextures.kf_texture,
                                                                   texcoord(1), texcoord(0));
        // Vec3<RealType> d_f_d_xy = intextures.dfdxy_texture(gl_FragCoord(1), gl_FragCoord(0));
        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>, TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture, texcoord(1), texcoord(0));

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //     return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_d_f = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        // Vec3<RealType> d_fexp_d_xy = d_fexp_d_f * d_f_d_xy;
        Vec3<RealType> d_fexp_d_xy = d_f_d_xy;

        RealType v0 = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        RealType v1 = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        RealType v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        Vec3<RealType> d_f_i_d_tra = Vec3<RealType>(v0, v1, v2);
        Vec3<RealType> d_f_i_d_rot = Vec3<RealType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        fragment.jtra = d_f_i_d_tra;
        fragment.jrot = d_f_i_d_rot;
        fragment.jexp = d_fexp_d_exp;
        fragment.image = f_exp;
    }

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
        Vec3<RealType> jtra = fragment.jtra;
        Vec3<RealType> jrot = fragment.jrot;
        Vec3<RealType> jexp = fragment.jexp;
        RealType image = fragment.image;

        textures.jtra_texture(y, x) = jtra;
        textures.jrot_texture(y, x) = jrot;
        textures.jexp_texture(y, x) = jexp;
        textures.image_texture(y, x) = image;
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class JPoseVelExpRendererBase
{
public:
    JPoseVelExpRendererBase() = delete;
    //~JPoseRendererBase() = default;

    struct InTextures
    {
        const TextureViewRead<ImageType> kf_texture;
        const TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jtra_texture;
        TextureViewWrite<Vec3<float>> jrot_texture;
        TextureViewWrite<Vec3<float>> jtravel_texture;
        TextureViewWrite<Vec3<float>> jrotvel_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        Vec6<RealType> vel_matrix;
        Vec2<RealType> exposure;
        PinholeCamera<RealType> camera;
        IntType out_width;
        IntType out_height;
        RealType readout_time;
    };

    struct Varyings
    {
        Vec3<RealType> kf_ver;
        Vec3<RealType> f_ver;
        RealType dt;
    };

    struct Fragment
    {
        Vec3<RealType> jtra;
        Vec3<RealType> jrot;
        Vec3<RealType> jtravel;
        Vec3<RealType> jrotvel;
        Vec3<RealType> jexp;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        return Fragment{Vec3<RealType>(textures.jtra_texture.nodata()),
                        Vec3<RealType>(textures.jrot_texture.nodata()),
                        Vec3<RealType>(textures.jtravel_texture.nodata()),
                        Vec3<RealType>(textures.jrotvel_texture.nodata()),
                        Vec3<RealType>(textures.jexp_texture.nodata()),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0, const Varyings &varying_px1, const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.dt =
            (w0 * varying_px0.dt +
             w1 * varying_px1.dt +
             w2 * varying_px2.dt);
        return var_over_w_px;
    }
    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        RealType dt; // = (vertexdata.texcoord(1) - RealType(0.5)) * uniforms.readout_time;
        Mat4<RealType> T_rs = (SE3<float>::exp(uniforms.vel_matrix * dt)).matrix() * uniforms.pose_matrix;
        Vec4<RealType> f_ver = T_rs * Vec4<RealType>(vertexdata.vertex(0),
                                                     vertexdata.vertex(1),
                                                     vertexdata.vertex(2),
                                                     RealType(1));
        gl_Position = uniforms.view_matrix * f_ver;

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ver = vertexdata.vertex;
        outVarying.dt = dt;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(kf_ver);

        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
            return;

        RealType kf = sample<RealType, TextureViewRead<ImageType>>(intextures.kf_texture,
                                                                   texcoord(1), texcoord(0));
        // Vec3<RealType> d_f_d_xy = intextures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>, TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture, texcoord(1), texcoord(0));

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //     return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_d_f = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_xy = d_fexp_d_f * d_f_d_xy;

        RealType v0 = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        RealType v1 = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        RealType v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        Vec3<RealType> d_f_i_d_tra = Vec3<RealType>(v0, v1, v2);
        Vec3<RealType> d_f_i_d_rot = Vec3<RealType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        Vec3<RealType> d_f_i_d_travel = in_varying.dt * d_f_i_d_tra;
        Vec3<RealType> d_f_i_d_rotvel = in_varying.dt * d_f_i_d_rot;

        fragment.jtra = d_f_i_d_tra;
        fragment.jrot = d_f_i_d_rot;
        fragment.jtravel = d_f_i_d_travel;
        fragment.jrotvel = d_f_i_d_rotvel;
        fragment.jexp = d_fexp_d_exp;
        fragment.image = f_exp;
    }

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
        Vec3<RealType> jtra = fragment.jtra;
        Vec3<RealType> jrot = fragment.jrot;
        Vec3<RealType> jtravel = fragment.jtravel;
        Vec3<RealType> jrotvel = fragment.jrotvel;
        Vec3<RealType> jexp = fragment.jexp;
        RealType image = fragment.image;

        textures.jtra_texture(y, x) = jtra;
        textures.jrot_texture(y, x) = jrot;
        textures.jtravel_texture(y, x) = jtravel;
        textures.jrotvel_texture(y, x) = jrotvel;
        textures.jexp_texture(y, x) = jexp;
        textures.image_texture(y, x) = image;
    }
};

template <template <class> class TextureView>
class PidsRendererBase
{
public:
    PidsRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        const int notused;
    };

    struct OutTextures
    {
        TextureView<Vec3<PidType>> pids_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        IntType out_width;
        IntType out_height;
    };

    struct Varyings
    {
        IntType vertexId;
        Vec3<IntType> pids;
    };

    struct Fragment
    {
        Vec3<IntType> pids;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        Vec3<PidType> pids_nodata(textures.pids_texture.nodata());

        return Fragment{Vec3<IntType>(pids_nodata(0), pids_nodata(1), pids_nodata(2))};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0, const Varyings &varying_px1, const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.pids = Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));
        gl_Position = uniforms.view_matrix * f_ver;

        outVarying.vertexId = vertexid;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        Vec3<IntType> vertexid = in_varying.pids;
        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));
        fragment.pids = ids;
    }

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
        Vec3<IntType> pids = fragment.pids;
        Vec3<PidType> pids_out(pids(0), pids(1), pids(2));
        textures.pids_texture(y, x) = pids_out;
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class JDepthExpRendererBase
{
public:
    JDepthExpRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        const TextureViewRead<ImageType> kf_texture;
        const TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jdepth_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<Vec3<PidType>> pids_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        PinholeCamera<RealType> camera;
        IntType out_width;
        IntType out_height;
        Vec2<RealType> exposure;
    };

    struct Varyings
    {
        Vec3<RealType> kf_ver;
        Vec3<RealType> f_ver;
        Vec3<RealType> kf_ray;
        Vec3<RealType> kf_ray_0;
        Vec3<RealType> kf_ray_1;
        Vec3<RealType> kf_ray_2;
        RealType depth;
        Vec3<RealType> baricentric;
        IntType vertexId;
        Vec3<IntType> pids;
    };

    struct Fragment
    {
        Vec3<RealType> jdepth;
        Vec3<RealType> jexp;
        Vec3<IntType> pids;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        Vec3<float> jdepth_nodata(textures.jdepth_texture.nodata());
        Vec3<float> jexp_nodata(textures.jexp_texture.nodata());
        Vec3<PidType> pids_nodata(textures.pids_texture.nodata());

        return Fragment{Vec3<RealType>(jdepth_nodata(0), jdepth_nodata(1), jdepth_nodata(2)),
                        Vec3<RealType>(jexp_nodata(0), jexp_nodata(1), jexp_nodata(2)),
                        Vec3<IntType>(pids_nodata(0), pids_nodata(1), pids_nodata(2)),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0, const Varyings &varying_px1, const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ray_0 = varying_px0.kf_ray;
        var_over_w_px.kf_ray_1 = varying_px1.kf_ray;
        var_over_w_px.kf_ray_2 = varying_px2.kf_ray;
        // var_over_w_px.barvout[2].screen(1)entric = Vec3<MathType>(w0 * invW0 * varying_px0.depth,
        //                                  w1 * invW1 * varying_px1.depth,
        //                                  w2 * invW2 * varying_px2.depth) *
        //                             (1.0f / invW_px);
        var_over_w_px.baricentric = Vec3<RealType>(w0,
                                                   w1,
                                                   w2);
        var_over_w_px.pids = Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));
        gl_Position = uniforms.view_matrix * f_ver;

        Vec3<RealType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), RealType(1));
        Vec4<RealType> d_f_ver_d_kf_depth_ = uniforms.pose_matrix * Vec4<RealType>(kf_ray(0),
                                                                                   kf_ray(1),
                                                                                   kf_ray(2),
                                                                                   RealType(0));
        Vec3<RealType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
        outVarying.kf_ver = vertexdata.vertex;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;
        Vec3<RealType> kf_ray_0 = in_varying.kf_ray_0;
        Vec3<RealType> kf_ray_1 = in_varying.kf_ray_1;
        Vec3<RealType> kf_ray_2 = in_varying.kf_ray_2;
        Vec3<RealType> baricentric = in_varying.baricentric;
        Vec3<IntType> vertexid = in_varying.pids;

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(kf_ver);

        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
            return;

        RealType kf = sample<RealType, TextureViewRead<ImageType>>(intextures.kf_texture, texcoord(1), texcoord(0));
        // Vec3<RealType> d_f_d_xy = intextures.dfdxy_texture(gl_FragCoord(1), gl_FragCoord(0));
        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>, TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture, texcoord(1), texcoord(0));

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //     return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_df = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        // Vec3<RealType> d_fexp_d_xy = d_fexp_df * d_f_d_xy;
        Vec3<RealType> d_fexp_d_xy = d_f_d_xy;

        Vec3<RealType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        d_f_i_d_f_ver(1) = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // Vec3<MathType>d_f_i_d_tra = Vec3<MathType>(v0, v1, v2);
        // Vec3<MathType>d_f_i_d_rot = Vec3<MathType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        RealType d_f_i_d_kf_depth_0 = (d_f_i_d_f_ver.transpose() * kf_ray_0)(0, 0);
        RealType d_f_i_d_kf_depth_1 = (d_f_i_d_f_ver.transpose() * kf_ray_1)(0, 0);
        RealType d_f_i_d_kf_depth_2 = (d_f_i_d_f_ver.transpose() * kf_ray_2)(0, 0);

        Vec3<RealType> jdepth;
        jdepth(0) = d_f_i_d_kf_depth_0 * baricentric(0);
        jdepth(1) = d_f_i_d_kf_depth_1 * baricentric(1);
        jdepth(2) = d_f_i_d_kf_depth_2 * baricentric(2);

        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        fragment.jdepth = jdepth;
        fragment.jexp = d_fexp_d_exp;
        fragment.pids = ids;
        fragment.image = f_exp;
    }

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
        Vec3<RealType> jdepth = fragment.jdepth;
        Vec3<RealType> jexp = fragment.jexp;
        Vec3<IntType> pids = fragment.pids;
        RealType image = fragment.image;

        Vec3<float> jdepth_out(jdepth(0), jdepth(1), jdepth(2));
        Vec3<float> jexp_out(jexp(0), jexp(1), jexp(2));
        Vec3<PidType> pids_out(pids(0), pids(1), pids(2));

        textures.jdepth_texture(y, x) = jdepth_out;
        textures.jexp_texture(y, x) = jexp_out;
        textures.pids_texture(y, x) = pids_out;
        textures.image_texture(y, x) = image;
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class JRayDepthExpRendererBase
{
public:
    JRayDepthExpRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        const TextureViewRead<ImageType> kf_texture;
        const TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jdepth_texture;
        TextureViewWrite<Vec3<float>> jray0_texture;
        TextureViewWrite<Vec3<float>> jray1_texture;
        TextureViewWrite<Vec3<float>> jray2_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<Vec3<PidType>> pids_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        PinholeCamera<RealType> camera;
        IntType out_width;
        IntType out_height;
        Vec2<RealType> exposure;
    };

    struct Varyings
    {
        Vec3<RealType> kf_ver;
        Vec3<RealType> f_ver;
        Vec3<RealType> kf_ray_0;
        Vec3<RealType> kf_ray_1;
        Vec3<RealType> kf_ray_2;
        RealType kf_depth_0;
        RealType kf_depth_1;
        RealType kf_depth_2;
        Vec3<RealType> baricentric;
        IntType vertexId;
        Vec3<IntType> pids;
    };

    struct Fragment
    {
        Vec3<RealType> jdepth;
        Vec3<RealType> jray0;
        Vec3<RealType> jray1;
        Vec3<RealType> jray2;
        Vec3<RealType> jexp;
        Vec3<IntType> pids;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        Vec3<float> jdepth_nodata(textures.jdepth_texture.nodata());
        Vec3<float> jray0_nodata(textures.jray0_texture.nodata());
        Vec3<float> jray1_nodata(textures.jray1_texture.nodata());
        Vec3<float> jray2_nodata(textures.jray2_texture.nodata());
        Vec3<float> jexp_nodata(textures.jexp_texture.nodata());
        Vec3<PidType> pids_nodata(textures.pids_texture.nodata());

        return Fragment{Vec3<RealType>(jdepth_nodata(0), jdepth_nodata(1), jdepth_nodata(2)),
                        Vec3<RealType>(jray0_nodata(0), jray0_nodata(1), jray0_nodata(2)),
                        Vec3<RealType>(jray1_nodata(0), jray1_nodata(1), jray1_nodata(2)),
                        Vec3<RealType>(jray2_nodata(0), jray2_nodata(1), jray2_nodata(2)),
                        Vec3<RealType>(jexp_nodata(0), jexp_nodata(1), jexp_nodata(2)),
                        Vec3<IntType>(pids_nodata(0), pids_nodata(1), pids_nodata(2)),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0, const Varyings &varying_px1, const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ray_0 = varying_px0.kf_ray_0;
        var_over_w_px.kf_ray_1 = varying_px1.kf_ray_0;
        var_over_w_px.kf_ray_2 = varying_px2.kf_ray_0;
        var_over_w_px.kf_depth_0 = varying_px0.kf_depth_0;
        var_over_w_px.kf_depth_1 = varying_px1.kf_depth_0;
        var_over_w_px.kf_depth_2 = varying_px2.kf_depth_0;
        // var_over_w_px.barvout[2].screen(1)entric = Vec3<MathType>(w0 * invW0 * varying_px0.depth,
        //                                  w1 * invW1 * varying_px1.depth,
        //                                  w2 * invW2 * varying_px2.depth) *
        //                             (1.0f / invW_px);
        var_over_w_px.baricentric = Vec3<RealType>(w0,
                                                   w1,
                                                   w2);
        var_over_w_px.pids = Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));
        gl_Position = uniforms.view_matrix * f_ver;

        Vec3<RealType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), RealType(1));

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ver = vertexdata.vertex;
        outVarying.kf_ray_0 = kf_ray;
        outVarying.kf_depth_0 = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;
        Vec3<RealType> kf_ray_0 = in_varying.kf_ray_0;
        Vec3<RealType> kf_ray_1 = in_varying.kf_ray_1;
        Vec3<RealType> kf_ray_2 = in_varying.kf_ray_2;
        RealType kf_depth_0 = in_varying.kf_depth_0;
        RealType kf_depth_1 = in_varying.kf_depth_1;
        RealType kf_depth_2 = in_varying.kf_depth_2;
        Vec3<RealType> bc = in_varying.baricentric;
        Vec3<IntType> vertexid = in_varying.pids;

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(kf_ver);

        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
            return;

        RealType kf = sample<RealType, TextureViewRead<ImageType>>(intextures.kf_texture, texcoord(1), texcoord(0));
        // Vec3<RealType> d_f_d_xy = intextures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>, TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture, texcoord(1), texcoord(0));

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //     return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_df = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_xy = d_fexp_df * d_f_d_xy;

        RealType v0 = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / kf_ver(2);
        RealType v1 = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / kf_ver(2);
        RealType v2 = -(v0 * kf_ver(0) + v1 * kf_ver(1)) / kf_ver(2);

        // Vec3<MathType>d_f_i_d_tra = Vec3<MathType>(v0, v1, v2);
        // Vec3<MathType>d_f_i_d_rot = Vec3<MathType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        Vec3<RealType> dI_d_kf = Vec3<RealType>(v0, v1, v2);

        // Vec3<RealType> dI_d_k = uniforms.inv_rot_matrix * dI_d_fver;

        // Depth jacobians: bc_i * (dI/dk · ray_i)
        RealType j_d0 = bc(0) * dI_d_kf.dot(kf_ray_0);
        RealType j_d1 = bc(1) * dI_d_kf.dot(kf_ray_1);
        RealType j_d2 = bc(2) * dI_d_kf.dot(kf_ray_2);
        Vec3<RealType> j_depth_012(j_d0, j_d1, j_d2);

        // Ray jacobians: bc_i * d_i * dI/dk  (component-wise)
        Vec3<RealType> j_r0 = bc(0) * kf_depth_0 * dI_d_kf;
        Vec3<RealType> j_r1 = bc(1) * kf_depth_1 * dI_d_kf;
        Vec3<RealType> j_r2 = bc(2) * kf_depth_2 * dI_d_kf;

        // Vec3<RealType> j_rayx_012(j_r0.x, j_r1.x, j_r2.x);
        // Vec3<RealType> j_rayy_012(j_r0.y, j_r1.y, j_r2.y);
        // Vec3<RealType> j_rayz_012(j_r0.z, j_r1.z, j_r2.z);

        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        fragment.jdepth = j_depth_012;
        fragment.jray0 = j_r0;
        fragment.jray1 = j_r1;
        fragment.jray2 = j_r2;
        fragment.jexp = d_fexp_d_exp;
        fragment.pids = ids;
        fragment.image = f_exp;
    }

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
        Vec3<RealType> jdepth = fragment.jdepth;
        Vec3<RealType> jray0 = fragment.jray0;
        Vec3<RealType> jray1 = fragment.jray1;
        Vec3<RealType> jray2 = fragment.jray2;
        Vec3<RealType> jexp = fragment.jexp;
        Vec3<IntType> pids = fragment.pids;
        RealType image = fragment.image;

        Vec3<float> jdepth_out(jdepth(0), jdepth(1), jdepth(2));
        Vec3<float> jray0_out(jray0(0), jray0(1), jray0(2));
        Vec3<float> jray1_out(jray1(0), jray1(1), jray1(2));
        Vec3<float> jray2_out(jray2(0), jray2(1), jray2(2));
        Vec3<float> jexp_out(jexp(0), jexp(1), jexp(2));
        Vec3<PidType> pids_out(pids(0), pids(1), pids(2));

        textures.jdepth_texture(y, x) = jdepth_out;
        textures.jray0_texture(y, x) = jray0_out;
        textures.jray1_texture(y, x) = jray1_out;
        textures.jray2_texture(y, x) = jray2_out;
        textures.jexp_texture(y, x) = jexp_out;
        textures.pids_texture(y, x) = pids_out;
        textures.image_texture(y, x) = image;
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class JVertexExpRendererBase
{
public:
    JVertexExpRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        const TextureViewRead<ImageType> kf_texture;
        const TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jv0_texture;
        TextureViewWrite<Vec3<float>> jv1_texture;
        TextureViewWrite<Vec3<float>> jv2_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<Vec3<PidType>> pids_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        Mat3<RealType> inv_rot_matrix;
        PinholeCamera<RealType> camera;
        IntType out_width;
        IntType out_height;
        Vec2<RealType> exposure;
    };

    struct Varyings
    {
        Vec3<RealType> kf_ver;
        Vec3<RealType> f_ver;
        Vec3<RealType> baricentric;
        IntType vertexId;
        Vec3<IntType> pids;
    };

    struct Fragment
    {
        Vec3<RealType> jv0;
        Vec3<RealType> jv1;
        Vec3<RealType> jv2;
        Vec3<RealType> jexp;
        Vec3<IntType> pids;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        Vec3<float> jv0_nodata(textures.jv0_texture.nodata());
        Vec3<float> jv1_nodata(textures.jv1_texture.nodata());
        Vec3<float> jv2_nodata(textures.jv2_texture.nodata());
        Vec3<float> jexp_nodata(textures.jexp_texture.nodata());
        Vec3<PidType> pids_nodata(textures.pids_texture.nodata());

        return Fragment{Vec3<RealType>(jv0_nodata(0), jv0_nodata(1), jv0_nodata(2)),
                        Vec3<RealType>(jv1_nodata(0), jv1_nodata(1), jv1_nodata(2)),
                        Vec3<RealType>(jv2_nodata(0), jv2_nodata(1), jv2_nodata(2)),
                        Vec3<RealType>(jexp_nodata(0), jexp_nodata(1), jexp_nodata(2)),
                        Vec3<IntType>(pids_nodata(0), pids_nodata(1), pids_nodata(2)),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0, const Varyings &varying_px1, const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        // var_over_w_px.barvout[2].screen(1)entric = Vec3<MathType>(w0 * invW0 * varying_px0.depth,
        //                                  w1 * invW1 * varying_px1.depth,
        //                                  w2 * invW2 * varying_px2.depth) *
        //                             (1.0f / invW_px);
        var_over_w_px.baricentric = Vec3<RealType>(w0,
                                                   w1,
                                                   w2);
        var_over_w_px.pids = Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));
        gl_Position = uniforms.view_matrix * f_ver;

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.vertexId = vertexid;
        outVarying.kf_ver = vertexdata.vertex;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;
        Vec3<RealType> baricentric = in_varying.baricentric;
        Vec3<IntType> vertexid = in_varying.pids;

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(kf_ver);

        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
            return;

        RealType kf = sample<RealType, TextureViewRead<ImageType>>(intextures.kf_texture, texcoord(1), texcoord(0));
        // Vec3<RealType> d_f_d_xy = intextures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), uniforms.out_lvl);
        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>, TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture, texcoord(1), texcoord(0));

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //     return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_df = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_xy = d_fexp_df * d_f_d_xy;

        Vec3<RealType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        d_f_i_d_f_ver(1) = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // Vec3<MathType>d_f_i_d_tra = Vec3<MathType>(v0, v1, v2);
        // Vec3<MathType>d_f_i_d_rot = Vec3<MathType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        // Convert to keyframe coordinates: dI/dk = R^T * dI/df
        Vec3<RealType> dI_d_kf = uniforms.inv_rot_matrix * d_f_i_d_f_ver;

        // Each vertex affects kf_ver by its barycentric weight
        Vec3<RealType> j_v0_xyz = baricentric(0) * dI_d_kf; // ∂I/∂(x0,y0,z0)
        Vec3<RealType> j_v1_xyz = baricentric(1) * dI_d_kf; // ∂I/∂(x1,y1,z1)
        Vec3<RealType> j_v2_xyz = baricentric(2) * dI_d_kf; // ∂I/∂(x2,y2,z2)

        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        fragment.jv0 = j_v0_xyz;
        fragment.jv1 = j_v1_xyz;
        fragment.jv2 = j_v2_xyz;
        fragment.jexp = d_fexp_d_exp;
        fragment.pids = ids;
        fragment.image = f_exp;
    }

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
        Vec3<RealType> jv0 = fragment.jv0;
        Vec3<RealType> jv1 = fragment.jv1;
        Vec3<RealType> jv2 = fragment.jv2;
        Vec3<RealType> jexp = fragment.jexp;
        Vec3<IntType> pids = fragment.pids;
        RealType image = fragment.image;

        Vec3<float> jv0_out(jv0(0), jv0(1), jv0(2));
        Vec3<float> jv1_out(jv1(0), jv1(1), jv1(2));
        Vec3<float> jv2_out(jv2(0), jv2(1), jv2(2));
        Vec3<float> jexp_out(jexp(0), jexp(1), jexp(2));
        Vec3<PidType> pids_out(pids(0), pids(1), pids(2));

        textures.jv0_texture(y, x) = jv0_out;
        textures.jv1_texture(y, x) = jv1_out;
        textures.jv2_texture(y, x) = jv2_out;
        textures.jexp_texture(y, x) = jexp_out;
        textures.pids_texture(y, x) = pids_out;
        textures.image_texture(y, x) = image;
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class JPoseExpDepthRendererBase
{
public:
    JPoseExpDepthRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        TextureViewRead<ImageType> kf_texture;
        TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jtra_texture;
        TextureViewWrite<Vec3<float>> jrot_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<Vec3<float>> jdepth_texture;
        TextureViewWrite<Vec3<PidType>> pids_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        Vec2<RealType> exposure;
        PinholeCamera<RealType> camera;
        IntType out_width;
        IntType out_height;
    };

    struct Varyings
    {
        Vec3<RealType> kf_ver;
        Vec3<RealType> f_ver;
        Vec3<RealType> kf_ray;
        Vec3<RealType> kf_ray_0;
        Vec3<RealType> kf_ray_1;
        Vec3<RealType> kf_ray_2;
        RealType depth;
        Vec3<RealType> baricentric;
        IntType vertexId;
        Vec3<IntType> pids;
    };

    struct Fragment
    {
        Vec3<RealType> jtra;
        Vec3<RealType> jrot;
        Vec3<RealType> jexp;
        Vec3<RealType> jdepth;
        Vec3<IntType> pids;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        Vec3<float> jtra_nodata(textures.jtra_texture.nodata());
        Vec3<float> jrot_nodata(textures.jrot_texture.nodata());
        Vec3<float> jexp_nodata(textures.jexp_texture.nodata());
        Vec3<float> jdepth_nodata(textures.jdepth_texture.nodata());
        Vec3<PidType> pids_nodata(textures.pids_texture.nodata());

        return Fragment{Vec3<RealType>(jtra_nodata(0), jtra_nodata(1), jtra_nodata(2)),
                        Vec3<RealType>(jrot_nodata(0), jrot_nodata(1), jrot_nodata(2)),
                        Vec3<RealType>(jexp_nodata(0), jexp_nodata(1), jexp_nodata(2)),
                        Vec3<RealType>(jdepth_nodata(0), jdepth_nodata(1), jdepth_nodata(2)),
                        Vec3<IntType>(pids_nodata(0), pids_nodata(1), pids_nodata(2)),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
#pragma HLS INLINE

        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0, const Varyings &varying_px1, const Varyings &varying_px2)
    {
#pragma HLS INLINE

        Varyings var_over_w_px;
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ray_0 = varying_px0.kf_ray;
        var_over_w_px.kf_ray_1 = varying_px1.kf_ray;
        var_over_w_px.kf_ray_2 = varying_px2.kf_ray;
        // var_over_w_px.barvout[2].screen(1)entric = Vec3<MathType>(w0 * invW0 * varying_px0.depth,
        //                                  w1 * invW1 * varying_px1.depth,
        //                                  w2 * invW2 * varying_px2.depth) *
        //                             (1.0f / invW_px);
        var_over_w_px.baricentric = Vec3<RealType>(w0,
                                                   w1,
                                                   w2);
        var_over_w_px.pids = Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
#pragma HLS INLINE

        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));

        gl_Position = uniforms.view_matrix * f_ver;

        Vec3<RealType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), RealType(1));
        Vec4<RealType> d_f_ver_d_kf_depth_ = uniforms.pose_matrix * Vec4<RealType>(kf_ray(0),
                                                                                   kf_ray(1),
                                                                                   kf_ray(2),
                                                                                   RealType(0));
        Vec3<RealType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
        outVarying.kf_ver = vertexdata.vertex;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
#pragma HLS INLINE

        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;
        Vec3<RealType> kf_ray_0 = in_varying.kf_ray_0;
        Vec3<RealType> kf_ray_1 = in_varying.kf_ray_1;
        Vec3<RealType> kf_ray_2 = in_varying.kf_ray_2;
        Vec3<RealType> baricentric = in_varying.baricentric;
        Vec3<IntType> vertexid = in_varying.pids;

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(kf_ver);
        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
            return;

        RealType kf = sample<RealType, TextureViewRead<ImageType>>(intextures.kf_texture,
                                                                   texcoord(1), texcoord(0));
        // Vec3<RealType> d_f_d_xy = intextures.dfdxy_texture(gl_FragCoord(1), gl_FragCoord(0));
        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>, TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture, texcoord(1), texcoord(0));

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //    return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_df = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_xy = d_fexp_df * d_f_d_xy;
        // Vec3<RealType> d_fexp_d_xy = d_f_d_xy;

        Vec3<RealType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        d_f_i_d_f_ver(1) = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        Vec3<RealType> d_f_i_d_tra = d_f_i_d_f_ver;
        Vec3<RealType> d_f_i_d_rot = Vec3<RealType>(-f_ver(2) * d_f_i_d_f_ver(1) + f_ver(1) * d_f_i_d_f_ver(2),
                                                    f_ver(2) * d_f_i_d_f_ver(0) - f_ver(0) * d_f_i_d_f_ver(2),
                                                    -f_ver(1) * d_f_i_d_f_ver(0) + f_ver(0) * d_f_i_d_f_ver(1));

        RealType d_f_i_d_kf_depth_0 = (d_f_i_d_f_ver.transpose() * kf_ray_0)(0, 0);
        RealType d_f_i_d_kf_depth_1 = (d_f_i_d_f_ver.transpose() * kf_ray_1)(0, 0);
        RealType d_f_i_d_kf_depth_2 = (d_f_i_d_f_ver.transpose() * kf_ray_2)(0, 0);

        Vec3<RealType> d_depth_d_vert_depth = baricentric;

        Vec3<RealType> jdepth;
        jdepth(0) = d_f_i_d_kf_depth_0 * d_depth_d_vert_depth(0);
        jdepth(1) = d_f_i_d_kf_depth_1 * d_depth_d_vert_depth(1);
        jdepth(2) = d_f_i_d_kf_depth_2 * d_depth_d_vert_depth(2);

        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        fragment.jtra = d_f_i_d_tra;
        fragment.jrot = d_f_i_d_rot;
        fragment.jexp = d_fexp_d_exp;
        fragment.jdepth = jdepth;
        fragment.pids = ids;
        fragment.image = f_exp;
    }

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
        Vec3<RealType> jtra = fragment.jtra;
        Vec3<RealType> jrot = fragment.jrot;
        Vec3<RealType> jexp = fragment.jexp;
        Vec3<RealType> jdepth = fragment.jdepth;
        Vec3<IntType> pids = fragment.pids;
        RealType image = fragment.image;

        Vec3<float> jtra_out(jtra(0), jtra(1), jtra(2));
        Vec3<float> jrot_out(jrot(0), jrot(1), jrot(2));
        Vec3<float> jexp_out(jexp(0), jexp(1), jexp(2));
        Vec3<float> jdepth_out(jdepth(0), jdepth(1), jdepth(2));
        Vec3<PidType> pids_out(pids(0), pids(1), pids(2));

        textures.jtra_texture(y, x) = jtra_out;
        textures.jrot_texture(y, x) = jrot_out;
        textures.jexp_texture(y, x) = jexp_out;
        textures.jdepth_texture(y, x) = jdepth_out;
        textures.pids_texture(y, x) = pids_out;
        textures.image_texture(y, x) = image;
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class JPoseVelExpDepthRendererBase
{
public:
    JPoseVelExpDepthRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        const TextureViewRead<ImageType> kf_texture;
        const TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jtra_texture;
        TextureViewWrite<Vec3<float>> jrot_texture;
        TextureViewWrite<Vec3<float>> jtravel_texture;
        TextureViewWrite<Vec3<float>> jrotvel_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<Vec3<float>> jdepth_texture;
        TextureViewWrite<Vec3<PidType>> pids_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        Vec6<RealType> vel_matrix;
        Vec2<RealType> exposure;
        PinholeCamera<RealType> camera;
        RealType readout_time;
        IntType out_width;
        IntType out_height;
    };

    struct Varyings
    {
        Vec3<RealType> kf_ver;
        Vec3<RealType> f_ver;
        Vec3<RealType> kf_ray;
        Vec3<RealType> kf_ray_0;
        Vec3<RealType> kf_ray_1;
        Vec3<RealType> kf_ray_2;
        RealType depth;
        Vec3<RealType> baricentric;
        IntType vertexId;
        Vec3<IntType> pids;
        RealType dt;
    };

    struct Fragment
    {
        Vec3<RealType> jtra;
        Vec3<RealType> jrot;
        Vec3<RealType> jtravel;
        Vec3<RealType> jrotvel;
        Vec3<RealType> jexp;
        Vec3<RealType> jdepth;
        Vec3<IntType> pids;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        Vec3<float> jtra_nodata(textures.jtra_texture.nodata());
        Vec3<float> jrot_nodata(textures.jrot_texture.nodata());
        Vec3<float> jtravel_nodata(textures.jtravel_texture.nodata());
        Vec3<float> jrotvel_nodata(textures.jrotvel_texture.nodata());
        Vec3<float> jexp_nodata(textures.jexp_texture.nodata());
        Vec3<float> jdepth_nodata(textures.jdepth_texture.nodata());
        Vec3<PidType> pids_nodata(textures.pids_texture.nodata());

        return Fragment{Vec3<RealType>(jtra_nodata(0), jtra_nodata(1), jtra_nodata(2)),
                        Vec3<RealType>(jrot_nodata(0), jrot_nodata(1), jrot_nodata(2)),
                        Vec3<RealType>(jtravel_nodata(0), jtravel_nodata(1), jtravel_nodata(2)),
                        Vec3<RealType>(jrotvel_nodata(0), jrotvel_nodata(1), jrotvel_nodata(2)),
                        Vec3<RealType>(jexp_nodata(0), jexp_nodata(1), jexp_nodata(2)),
                        Vec3<RealType>(jdepth_nodata(0), jdepth_nodata(1), jdepth_nodata(2)),
                        Vec3<IntType>(pids_nodata(0), pids_nodata(1), pids_nodata(2)),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &verte_buffer, const IntType vertexid)
    {
#pragma HLS inline

        VertexData vertexdata;

        IntType base = vertexid * 3;

        vertexdata.vertex(0) = verte_buffer[base + 0];
        vertexdata.vertex(1) = verte_buffer[base + 1];
        vertexdata.vertex(2) = verte_buffer[base + 2];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0, const Varyings &varying_px1, const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ray_0 = varying_px0.kf_ray;
        var_over_w_px.kf_ray_1 = varying_px1.kf_ray;
        var_over_w_px.kf_ray_2 = varying_px2.kf_ray;
        // var_over_w_px.barvout[2].screen(1)entric = Vec3<MathType>(w0 * invW0 * varying_px0.depth,
        //                                  w1 * invW1 * varying_px1.depth,
        //                                  w2 * invW2 * varying_px2.depth) *
        //                             (1.0f / invW_px);
        var_over_w_px.baricentric = Vec3<RealType>(w0,
                                                   w1,
                                                   w2);
        var_over_w_px.pids = Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);
        var_over_w_px.dt =
            (w0 * varying_px0.dt +
             w1 * varying_px1.dt +
             w2 * varying_px2.dt);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
        RealType dt; // = (vertexdata.texcoord(1) - RealType(0.5)) * uniforms.readout_time;
        Mat4<RealType> T_rs = (SE3<float>::exp(uniforms.vel_matrix * dt)).matrix() * uniforms.pose_matrix;
        Vec4<RealType> f_ver = T_rs * Vec4<RealType>(vertexdata.vertex(0),
                                                     vertexdata.vertex(1),
                                                     vertexdata.vertex(2),
                                                     RealType(1));

        gl_Position = uniforms.view_matrix * f_ver;

        Vec3<RealType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), RealType(1));
        Vec4<RealType> d_f_ver_d_kf_depth_ = T_rs * Vec4<RealType>(kf_ray(0),
                                                                   kf_ray(1),
                                                                   kf_ray(2),
                                                                   RealType(0));
        Vec3<RealType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
        outVarying.kf_ver = vertexdata.vertex;
        outVarying.dt = dt;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;
        Vec3<RealType> kf_ray_0 = in_varying.kf_ray_0;
        Vec3<RealType> kf_ray_1 = in_varying.kf_ray_1;
        Vec3<RealType> kf_ray_2 = in_varying.kf_ray_2;
        Vec3<RealType> baricentric = in_varying.baricentric;
        Vec3<IntType> vertexid = in_varying.pids;

        Vec2<RealType> texcoord = uniforms.camera.pointToPix(kf_ver);
        if (texcoord(0) < RealType(0) || texcoord(0) > RealType(1) ||
            texcoord(1) < RealType(0) || texcoord(1) > RealType(1))
            return;

        RealType kf = sample<RealType, TextureViewRead<ImageType>>(intextures.kf_texture,
                                                                   texcoord(1), texcoord(0));

        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>, TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture, texcoord(1), texcoord(0));

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //     return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_df = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_xy = d_fexp_df * d_f_d_xy;

        Vec3<RealType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        d_f_i_d_f_ver(1) = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        Vec3<RealType> d_f_i_d_tra = d_f_i_d_f_ver;
        Vec3<RealType> d_f_i_d_rot = Vec3<RealType>(-f_ver(2) * d_f_i_d_f_ver(1) + f_ver(1) * d_f_i_d_f_ver(2),
                                                    f_ver(2) * d_f_i_d_f_ver(0) - f_ver(0) * d_f_i_d_f_ver(2),
                                                    -f_ver(1) * d_f_i_d_f_ver(0) + f_ver(0) * d_f_i_d_f_ver(1));

        Vec3<RealType> d_f_i_d_travel = in_varying.dt * d_f_i_d_tra;
        Vec3<RealType> d_f_i_d_rotvel = in_varying.dt * d_f_i_d_rot;

        RealType d_f_i_d_kf_depth_0 = (d_f_i_d_f_ver.transpose() * kf_ray_0)(0, 0);
        RealType d_f_i_d_kf_depth_1 = (d_f_i_d_f_ver.transpose() * kf_ray_1)(0, 0);
        RealType d_f_i_d_kf_depth_2 = (d_f_i_d_f_ver.transpose() * kf_ray_2)(0, 0);

        Vec3<RealType> d_depth_d_vert_depth = baricentric;

        Vec3<RealType> jdepth;
        jdepth(0) = d_f_i_d_kf_depth_0 * d_depth_d_vert_depth(0);
        jdepth(1) = d_f_i_d_kf_depth_1 * d_depth_d_vert_depth(1);
        jdepth(2) = d_f_i_d_kf_depth_2 * d_depth_d_vert_depth(2);

        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        fragment.jtra = d_f_i_d_tra;
        fragment.jrot = d_f_i_d_rot;
        fragment.jtravel = d_f_i_d_travel;
        fragment.jrotvel = d_f_i_d_rotvel;
        fragment.jexp = d_fexp_d_exp;
        fragment.jdepth = jdepth;
        fragment.pids = ids;
        fragment.image = f_exp;
    }

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
        Vec3<RealType> jtra = fragment.jtra;
        Vec3<RealType> jrot = fragment.jrot;
        Vec3<RealType> jtravel = fragment.jtravel;
        Vec3<RealType> jrotvel = fragment.jrotvel;
        Vec3<RealType> jexp = fragment.jexp;
        Vec3<RealType> jdepth = fragment.jdepth;
        Vec3<IntType> pids = fragment.pids;
        RealType image = fragment.image;

        Vec3<float> jtra_out(jtra(0), jtra(1), jtra(2));
        Vec3<float> jrot_out(jrot(0), jrot(1), jrot(2));
        Vec3<float> jtravel_out(jtravel(0), jtravel(1), jtravel(2));
        Vec3<float> jrotvel_out(jrotvel(0), jrotvel(1), jrotvel(2));
        Vec3<float> jexp_out(jexp(0), jexp(1), jexp(2));
        Vec3<float> jdepth_out(jdepth(0), jdepth(1), jdepth(2));
        Vec3<PidType> pids_out(pids(0), pids(1), pids(2));

        textures.jtra_texture(y, x) = jtra_out;
        textures.jrot_texture(y, x) = jrot_out;
        textures.jtravel_texture(y, x) = jtravel_out;
        textures.jrotvel_texture(y, x) = jrotvel_out;
        textures.jexp_texture(y, x) = jexp_out;
        textures.jdepth_texture(y, x) = jdepth_out;
        textures.pids_texture(y, x) = pids_out;
        textures.image_texture(y, x) = image;
    }
};

template <template <class> class TextureViewRead,
          template <class> class TextureViewWrite>
class DiffRendererBase
{
public:
    DiffRendererBase() = delete;
    //~JMapRendererBase() = default;

    struct InTextures
    {
        TextureViewRead<ImageType> diffuse_texture;
        TextureViewRead<Vec3<float>> dfdxy_texture;
    };

    struct OutTextures
    {
        TextureViewWrite<Vec3<float>> jtra_texture;
        TextureViewWrite<Vec3<float>> jrot_texture;
        TextureViewWrite<Vec3<float>> jexp_texture;
        TextureViewWrite<Vec3<float>> jmap_texture;
        TextureViewWrite<Vec3<PidType>> pids_texture;
        TextureViewWrite<ImageType> image_texture;
    };

    struct VertexData
    {
        Vec3<RealType> vertex;
        Vec2<RealType> texcoord;
    };

    struct Uniforms
    {
        RealType fx;
        RealType fy;
        Mat4<RealType> view_matrix;
        Mat4<RealType> pose_matrix;
        Vec2<RealType> exposure;
        IntType out_width;
        IntType out_height;
    };

    struct Varyings
    {
        Vec2<RealType> texcoord;
        Vec3<RealType> kf_ver;
        Vec3<RealType> f_ver;
        Vec3<RealType> kf_ray_0;
        Vec3<RealType> kf_ray_1;
        Vec3<RealType> kf_ray_2;
        Vec3<RealType> baricentric;
        IntType vertexId;
        Vec3<IntType> pids;
    };

    struct Fragment
    {
        Vec3<RealType> jtra;
        Vec3<RealType> jrot;
        Vec3<RealType> jexp;
        Vec3<RealType> jmap;
        Vec3<IntType> pids;
        RealType image;
    };

    static Fragment fragment_nodata(OutTextures &textures)
    {
#pragma HLS inline

        Vec3<float> jtra_nodata(textures.jtra_texture.nodata());
        Vec3<float> jrot_nodata(textures.jrot_texture.nodata());
        Vec3<float> jexp_nodata(textures.jexp_texture.nodata());
        Vec3<float> jmap_nodata(textures.jmap_texture.nodata());
        Vec3<PidType> pids_nodata(textures.pids_texture.nodata());

        return Fragment{Vec3<RealType>(jtra_nodata(0), jtra_nodata(1), jtra_nodata(2)),
                        Vec3<RealType>(jrot_nodata(0), jrot_nodata(1), jrot_nodata(2)),
                        Vec3<RealType>(jexp_nodata(0), jexp_nodata(1), jexp_nodata(2)),
                        Vec3<RealType>(jmap_nodata(0), jmap_nodata(1), jmap_nodata(2)),
                        Vec3<IntType>(pids_nodata(0), pids_nodata(1), pids_nodata(2)),
                        RealType(textures.image_texture.nodata())};
    }

    template <class BufferView>
    static VertexData get_vertex_data(const BufferView &vertex_buffer, const IntType vertexid)
    {
#pragma HLS INLINE

        VertexData vertexdata;

        IntType base = vertexid * 5;

        vertexdata.vertex(0) = vertex_buffer[base + 0];
        vertexdata.vertex(1) = vertex_buffer[base + 1];
        vertexdata.vertex(2) = vertex_buffer[base + 2];

        vertexdata.texcoord(0) = vertex_buffer[base + 3];
        vertexdata.texcoord(1) = vertex_buffer[base + 4];

        return vertexdata;
    }

    static Varyings interpolate_varyings(IntType triangle_id,
                                         const RealType w0, const RealType w1, const RealType w2,
                                         const Varyings &varying_px0, const Varyings &varying_px1, const Varyings &varying_px2)
    {
#pragma HLS INLINE

        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord +
             w1 * varying_px1.texcoord +
             w2 * varying_px2.texcoord);
        var_over_w_px.kf_ver =
            (w0 * varying_px0.kf_ver +
             w1 * varying_px1.kf_ver +
             w2 * varying_px2.kf_ver);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ray_0 = varying_px0.kf_ray_0;
        var_over_w_px.kf_ray_1 = varying_px1.kf_ray_0;
        var_over_w_px.kf_ray_2 = varying_px2.kf_ray_0;
        // var_over_w_px.barvout[2].screen(1)entric = Vec3<MathType>(w0 * invW0 * varying_px0.depth,
        //                                  w1 * invW1 * varying_px1.depth,
        //                                  w2 * invW2 * varying_px2.depth) *
        //                             (1.0f / invW_px);
        var_over_w_px.baricentric = Vec3<RealType>(w0,
                                                   w1,
                                                   w2);
        var_over_w_px.pids = Vec3<IntType>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    static void vertex_shader(const VertexData &vertexdata,
                              const IntType &vertexid,
                              const Uniforms &uniforms,
                              Vec4<RealType> &gl_Position,
                              Varyings &outVarying)
    {
#pragma HLS INLINE

        Vec4<RealType> f_ver = uniforms.pose_matrix * Vec4<RealType>(vertexdata.vertex(0),
                                                                     vertexdata.vertex(1),
                                                                     vertexdata.vertex(2),
                                                                     RealType(1));

        gl_Position = uniforms.view_matrix * f_ver;

        Vec3<RealType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), RealType(1));
        Vec4<RealType> d_f_ver_d_kf_depth_ = uniforms.pose_matrix * Vec4<RealType>(kf_ray(0),
                                                                                   kf_ray(1),
                                                                                   kf_ray(2),
                                                                                   RealType(0));
        Vec3<RealType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = Vec3<RealType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray_0 = d_f_ver_d_kf_depth;
        outVarying.vertexId = vertexid;
        outVarying.texcoord = vertexdata.texcoord;
        outVarying.kf_ver = vertexdata.vertex;
    }

    static void fragment_shader(const Vec4<RealType> &gl_FragCoord,
                                const Uniforms &uniforms,
                                const Varyings &in_varying,
                                const InTextures &intextures,
                                Fragment &fragment)
    {
#pragma HLS INLINE

        IntType out_width = uniforms.out_width;
        IntType out_height = uniforms.out_height;

        Vec3<RealType> kf_ver = in_varying.kf_ver;
        Vec3<RealType> f_ver = in_varying.f_ver;
        Vec3<RealType> kf_ray_0 = in_varying.kf_ray_0;
        Vec3<RealType> kf_ray_1 = in_varying.kf_ray_1;
        Vec3<RealType> kf_ray_2 = in_varying.kf_ray_2;
        Vec2<RealType> texcoord = in_varying.texcoord;
        Vec3<RealType> baricentric = in_varying.baricentric;
        Vec3<IntType> vertexid = in_varying.pids;

        RealType kf = sample<RealType,
                             TextureViewRead<ImageType>>(intextures.diffuse_texture,
                                                         texcoord(1), texcoord(0),
                                                         AddressMode::Clamp,
                                                         FilterMode::Nearest);

        Vec3<RealType> d_f_d_xy = sample<Vec3<RealType>,
                                         TextureViewRead<Vec3<float>>>(intextures.dfdxy_texture,
                                                                       texcoord(1), texcoord(0),
                                                                       AddressMode::Clamp,
                                                                       FilterMode::Nearest);

        // if (kf == intextures.kf_texture.nodata() || f == intextures.f_texture.nodata() || d_f_d_xy == intextures.dfdxy_texture.nodata())
        //    return;

        RealType f_exp = apply_exposure(RealType(kf), uniforms.exposure);
        RealType d_fexp_df = d_f_exp_d_f(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_exp = d_f_exp_d_exp(RealType(kf), uniforms.exposure);
        Vec3<RealType> d_fexp_d_xy = d_fexp_df * d_f_d_xy;

        Vec3<RealType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = d_fexp_d_xy(0) * uniforms.fx * RealType(out_width) / f_ver(2);
        d_f_i_d_f_ver(1) = d_fexp_d_xy(1) * uniforms.fy * RealType(out_height) / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        Vec3<RealType> d_f_i_d_tra = d_f_i_d_f_ver;
        Vec3<RealType> d_f_i_d_rot = Vec3<RealType>(-f_ver(2) * d_f_i_d_f_ver(1) + f_ver(1) * d_f_i_d_f_ver(2),
                                                    f_ver(2) * d_f_i_d_f_ver(0) - f_ver(0) * d_f_i_d_f_ver(2),
                                                    -f_ver(1) * d_f_i_d_f_ver(0) + f_ver(0) * d_f_i_d_f_ver(1));

        RealType d_f_i_d_kf_depth_0 = (d_f_i_d_f_ver.transpose() * kf_ray_0)(0, 0);
        RealType d_f_i_d_kf_depth_1 = (d_f_i_d_f_ver.transpose() * kf_ray_1)(0, 0);
        RealType d_f_i_d_kf_depth_2 = (d_f_i_d_f_ver.transpose() * kf_ray_2)(0, 0);

        Vec3<RealType> d_depth_d_vert_depth = baricentric;

        Vec3<RealType> jac;
        jac(0) = d_f_i_d_kf_depth_0 * d_depth_d_vert_depth(0);
        jac(1) = d_f_i_d_kf_depth_1 * d_depth_d_vert_depth(1);
        jac(2) = d_f_i_d_kf_depth_2 * d_depth_d_vert_depth(2);

        Vec3<IntType> ids = Vec3<IntType>(vertexid(0), vertexid(1), vertexid(2));

        fragment.jtra = d_f_i_d_tra;
        fragment.jrot = d_f_i_d_rot;
        fragment.jexp = d_fexp_d_exp;
        fragment.jmap = jac;
        fragment.pids = ids;
        fragment.image = f_exp;
    }

    static void set_outtexture(const Fragment &fragment, OutTextures &textures, IntType x, IntType y)
    {
#pragma HLS INLINE

        Vec3<RealType> jtra = fragment.jtra;
        Vec3<RealType> jrot = fragment.jrot;
        Vec3<RealType> jexp = fragment.jexp;
        Vec3<RealType> jmap = fragment.jmap;
        Vec3<IntType> pids = fragment.pids;
        RealType image = fragment.image;

        Vec3<float> jtra_out(jtra(0), jtra(1), jtra(2));
        Vec3<float> jrot_out(jrot(0), jrot(1), jrot(2));
        Vec3<float> jexp_out(jexp(0), jexp(1), jexp(2));
        Vec3<float> jmap_out(jmap(0), jmap(1), jmap(2));
        Vec3<PidType> pids_out(pids(0), pids(1), pids(2));

        textures.jtra_texture(y, x) = jtra_out;
        textures.jrot_texture(y, x) = jrot_out;
        textures.jexp_texture(y, x) = jexp_out;
        textures.jmap_texture(y, x) = jmap_out;
        textures.pids_texture(y, x) = pids_out;
        textures.image_texture(y, x) = image;
    }
};
