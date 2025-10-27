#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
#include "linalg/linalg.h"
#include "backends/base/texturebase.h"
#include "core/render_constants.h"
// #include "core/error_handling.h"

// template <typename T>
// inline T cross(const linalg::Vec2<T> &a, const linalg::Vec2<T> &b) { return a(0) * b(1) - a(1) * b(0); }

// template <typename T>
// inline T triangle_area(const linalg::Vec2<T> &p0, const linalg::Vec2<T> &p1, const linalg::Vec2<T> &p2) { return cross<T>(p1 - p0, p2 - p0); }

// Edge function E_ab(p) = (vout[1].screen(1)-vout[0].screen(1))*px + (vout[0].screen(0)-vout[1].screen(0))*py + (vout[1].screen(0)*vout[0].screen(1) - vout[0].screen(0)*vout[1].screen(1))
template <typename T>
T edge_func(const linalg::Vec2<T> &v0, const linalg::Vec2<T> &v1, const linalg::Vec2<T> &v2)
{
    // return (y1 - y0) * (px - x0) + (x0 - x1) * (py - y0);
    //  return (by - ay) * px + (ax - bx) * py + (bx * ay - ax * by);
    linalg::Vec2<T> v10 = v1 - v0;
    linalg::Vec2<T> v20 = v2 - v0;
    // for y up
    // return v10.cross(v20);
    // for y down
    return v20.cross(v10);
    // return (v2(1) - v0(1)) * (v1(0) - v0(0)) - (v2(0) - v0(0)) * (v1(1) - v0(1));
}

// Top-left test: returns true if edge is a "top" or "left" edge
template <typename T>
bool is_top_left(const linalg::Vec2<T> &v0, const linalg::Vec2<T> &v1)
{
    // return (v0(1) == v1(1)) ? (v1(0) < v0(0)) : (v0(1) < v1(1));
    // for y up
    // return (v0(1) < v1(1)) || (v0(1) == v1(1) && v0(0) > v1(0));
    // for y down
    return (v0(1) > v1(1)) || (v0(1) == v1(1) && v0(0) > v1(0));
}

// -----------------------------------------------------------------------------
// RendererBase
// -----------------------------------------------------------------------------
template <typename MathType, class Derived>
class RendererBase
{
public:
    RendererBase()
    {
        opencv2opengl_ = linalg::Mat4<MathType>::Identity();
        opencv2opengl_(1, 1) = -1.0; // flip Z like your original intent
        opencv2opengl_(2, 2) = -1.0; // flip Z like your original intent
    };
    // virtual ~RendererBase() = default;
    ~RendererBase() = default;

    template <typename Mesh, typename Textures>
    void Render(const BoundingBox<int> &viewport,
                const Mesh &mesh,
                Textures &textures)
    {
    // ---- Map mesh buffers (no copies) ----
    // auto pos = mesh.MapReadPositions(); // 3 floats/vertex
    // auto tex = mesh.MapReadTevout[2].screen(0)oords(); // 2 floats/vertex
    // auto wei = mesh.MapReadWeights();   // 1 float /vertex
    // auto idx = mesh.MapReadIndices();   // uint32_t indices

    // const auto pos = mesh.Positions(); // 3 floats/vertex
    // const auto tex = mesh.Tevout[2].screen(0)oords(); // 2 floats/vertex
    // const auto wei = mesh.Weights();   // 1 float /vertex
    // const auto idx = mesh.Indices();   // uint32_t indices

    // Loop over triangles
    renderbase_triangle_loop:
        for (unsigned int i = 0; i + 2 < mesh.ebo_buffer_.size(); i += 3)
        {
            // the amount of in a 32x32 mesh (31x31*2)
#pragma HLS loop_tripcount min = 1922 max = 1922 avg = 1922
            // #pragma HLS PIPELINE II = 1

            unsigned int vertexids[3];

            vertexids[0] = mesh.ebo_buffer_[i + 0];
            vertexids[1] = mesh.ebo_buffer_[i + 1];
            vertexids[2] = mesh.ebo_buffer_[i + 2];

            typename Derived::VertexData vertexdata[3];

            vertexdata[0] = derived_().get_vertex_data(mesh, vertexids[0]);
            vertexdata[1] = derived_().get_vertex_data(mesh, vertexids[1]);
            vertexdata[2] = derived_().get_vertex_data(mesh, vertexids[2]);

            /*
        renderbase_gather_loop:
            for (int k = 0; k < 3; ++k)
            {
#pragma HLS UNROLL
                unsigned int vi = (k == 0 ? i0 : k == 1 ? i1
                                                        : i2);
                v[k](0) = mesh.pos_buffer_[vi * 3 + 0];
                v[k](1) = mesh.pos_buffer_[vi * 3 + 1];
                v[k](2) = mesh.pos_buffer_[vi * 3 + 2];
                uv[k](0) = mesh.tex_buffer_[vi * 2 + 0];
                uv[k](1) = mesh.tex_buffer_[vi * 2 + 1];
                wght[k] = mesh.wei_buffer_[vi];
            }
            */

            this->draw_triangle_(vertexdata, vertexids, viewport, textures);
        }
    }

protected:
    // Triangle rasterizer (top-left rule, perspective correct)
    template <typename VertexData, typename Textures>
    void draw_triangle_(const VertexData *vertexdata,
                        const unsigned int *vertexid,
                        const BoundingBox<int> &viewport,
                        Textures &textures)
    {
        // Vertex shading & clip → NDC → screen
        struct VSOut
        {
            linalg::Vec2<MathType> screen; // x,y in pixel space (float)
            MathType depth;                // z in [0,1] if your projection is like GL_ZERO_TO_ONE
            MathType invW;                 // 1 / clip.w
            // std::tuple<Varyings...> var_over_w; // varyings multiplied by invW
            typename Derived::Varyings var; // original varyings (for convenience)
        } vout[3];

        const MathType vp_w = static_cast<MathType>(viewport.max_x_ - viewport.min_x_);
        const MathType vp_h = static_cast<MathType>(viewport.max_y_ - viewport.min_y_);

    draw_triangle_vertex_loop:
        for (int i = 0; i < 3; ++i)
        {
#pragma HLS UNROLL

            linalg::Vec4<MathType> gl_Position;
            typename Derived::Varyings varyings;
            derived_().vertex_shader(vertexdata[i], vertexid[i], gl_Position, varyings);

            const MathType invW = MathType(1) / gl_Position(3);
            const MathType ndc_x = gl_Position(0) * invW; // [-1,1]
            const MathType ndc_y = gl_Position(1) * invW;
            const MathType ndc_z = gl_Position(2) * invW; // assumed 0..1 after proj (adjust if -1..1)

            // pixel-space (don’t clamp here) — match GL rasterization (remove +1/-0.5 adjustment)
            vout[i].screen(0) = MathType(0.5) * (ndc_x + MathType(1)) * vp_w + viewport.min_x_;
            vout[i].screen(1) = MathType(0.5) * (ndc_y + MathType(1)) * vp_h + viewport.min_y_;
            vout[i].depth = ndc_z;
            vout[i].invW = invW;
            vout[i].var = varyings;
            // vout[i].var_over_w = varyings * invW; // requires T*VaryingType
        }

        // Back-face cull (optional). Keep CCW (area > 0) – adjust sign to your convention
        // const MathType area = triangle_area<MathType>(vout[0].screen,
        //                                              vout[1].screen,
        //                                              vout[2].screen);
        MathType area2 = edge_func(vout[0].screen, vout[1].screen, vout[2].screen); // 2*area with sign

        if (area2 < MathType(0))
            return; // enable to cull backfaces

        /*
    // Enforce CCW so the inside test is consistent
    if (area2 < MathType(0))
    {
        std::swap(vout[1], vout[2]);
        // area2 = edge(X(0), Y(0), X(1), Y(1), X(2), Y(2));
        area2 = edge_func(vout[0].screen, vout[1].screen, vout[2].screen);
        if (area2 <= MathType(0))
            return; // still degenerate
    }
    */

        // Triangle bounding box (float → int, clamp to viewport)
        MathType minx = min(min(vout[0].screen(0), vout[1].screen(0)), vout[2].screen(0));
        MathType maxx = max(max(vout[0].screen(0), vout[1].screen(0)), vout[2].screen(0));
        MathType miny = min(min(vout[0].screen(1), vout[1].screen(1)), vout[2].screen(1));
        MathType maxy = max(max(vout[0].screen(1), vout[1].screen(1)), vout[2].screen(1));

        int x0 = max(viewport.min_x_, static_cast<int>(floor(minx)));
        int x1 = min(viewport.max_x_, static_cast<int>(ceil(maxx)));
        int y0 = max(viewport.min_y_, static_cast<int>(floor(miny)));
        int y1 = min(viewport.max_y_, static_cast<int>(ceil(maxy)));
        if (x0 >= x1 || y0 >= y1)
            return;

        const MathType inv_area2 = MathType(1) / area2;

        // Edge setup (top-left rule)
        // const MathType vout[0].screen(0) = vout[0].screen(0), vout[0].screen(1) = vout[0].screen(1);
        // const MathType vout[1].screen(0) = vout[1].screen(0), vout[1].screen(1) = vout[1].screen(1);
        // const MathType vout[2].screen(0) = vout[2].screen(0), vout[2].screen(1) = vout[2].screen(1);

        // const MathType area2 = edge_func(vout[0].screen(0), vout[0].screen(1), vout[1].screen(0), vout[1].screen(1), vout[2].screen(0), vout[2].screen(1)); // 2*area with sign
        // ErrorHandling::ValidateTriangleArea(area2);
        // ErrorHandling::ValidateNonZero(area2, "triangle area calculation");

        const bool tlAB = is_top_left(vout[0].screen, vout[1].screen);
        const bool tlBC = is_top_left(vout[1].screen, vout[2].screen);
        const bool tlCA = is_top_left(vout[2].screen, vout[0].screen);

        // Evaluate edge functions at top-left corner of each pixel (add +0.5)
        linalg::Vec2<MathType> p;
        p(0) = static_cast<MathType>(x0) + MathType(RenderConstants::PIXEL_CENTER_OFFSET);
        p(1) = static_cast<MathType>(y0) + MathType(RenderConstants::PIXEL_CENTER_OFFSET);

        MathType eAB_row = edge_func(vout[0].screen, vout[1].screen, p);
        MathType eBC_row = edge_func(vout[1].screen, vout[2].screen, p);
        MathType eCA_row = edge_func(vout[2].screen, vout[0].screen, p);

        // MathType eAB = eAB_row;
        // MathType eBC = eBC_row;
        // MathType eCA = eCA_row;

        // Step increments when moving +1 in X or +1 in Y
        // const MathType eAB_dx = (vout[0].screen(1) - vout[1].screen(1));
        // const MathType eAB_dy = (vout[1].screen(0) - vout[0].screen(0));
        // const MathType eBC_dx = (vout[1].screen(1) - vout[2].screen(1));
        // const MathType eBC_dy = (vout[2].screen(0) - vout[1].screen(0));
        // const MathType eCA_dx = (vout[2].screen(1) - vout[0].screen(1));
        // const MathType eCA_dy = (vout[0].screen(0) - vout[2].screen(0));
        // for y down, the - is needed
        const MathType eAB_dx = (vout[1].screen(1) - vout[0].screen(1));
        const MathType eAB_dy = (vout[0].screen(0) - vout[1].screen(0));
        const MathType eBC_dx = (vout[2].screen(1) - vout[1].screen(1));
        const MathType eBC_dy = (vout[1].screen(0) - vout[2].screen(0));
        const MathType eCA_dx = (vout[0].screen(1) - vout[2].screen(1));
        const MathType eCA_dy = (vout[2].screen(0) - vout[0].screen(0));

        derived_().read_cache(y0, y1, x0, x1, textures);

    // Rasterize
    draw_triangle_raster_loop_y:
        for (int y = y0, iy = 0; y < y1; ++y, ++iy)
        {
            // for 32x32 meshes and 640x480 images
#pragma HLS loop_tripcount min = 15 max = 15 avg = 15
            // #pragma HLS PIPELINE II = 1
            //   MathType eAB = eAB_row;
            //   MathType eBC = eBC_row;
            //   MathType eCA = eCA_row;

            const MathType eAB_row_local = MathType(iy) * eAB_dy + eAB_row;
            const MathType eBC_row_local = MathType(iy) * eBC_dy + eBC_row;
            const MathType eCA_row_local = MathType(iy) * eCA_dy + eCA_row;

            // const MathType eAB_row_local = eAB_row;
            // const MathType eBC_row_local = eBC_row;
            // const MathType eCA_row_local = eCA_row;

        draw_triangle_raster_loop_x:
            for (int x = x0, ix = 0; x < x1; ++x, ++ix)
            {
                // for 32x32 meshes and 640x480 images
#pragma HLS loop_tripcount min = 20 max = 20 avg = 20
#pragma HLS loop_flatten
                //   #pragma HLS PIPELINE II = 1

                const MathType eAB = MathType(ix) * eAB_dx + eAB_row_local;
                const MathType eBC = MathType(ix) * eBC_dx + eBC_row_local;
                const MathType eCA = MathType(ix) * eCA_dx + eCA_row_local;

                // Top-left rule adjustments (include pixels on top/left edges)
                const bool inside =
                    (eAB > 0 || (eAB == 0 && tlAB)) &&
                    (eBC > 0 || (eBC == 0 && tlBC)) &&
                    (eCA > 0 || (eCA == 0 && tlCA));

                // Baricentric weights normalized
                // const MathType w0 = eBC * inv_area2;
                // const MathType w1 = eCA * inv_area2;
                // const MathType w2 = eAB * inv_area2;
                // Baricentric weights normalized (perpective)
                MathType w0 = eBC * inv_area2 * vout[0].invW;
                MathType w1 = eCA * inv_area2 * vout[1].invW;
                MathType w2 = eAB * inv_area2 * vout[2].invW;

                // Perspective: 1/w at pixel
                const MathType inv_invW_px = MathType(1) / (w0 + w1 + w2);

                w0 *= inv_invW_px;
                w1 *= inv_invW_px;
                w2 *= inv_invW_px;

                typename Derived::Varyings varying_px = derived_().interpolate_varyings(w0, w1, w2,
                                                                                        vout[0].var, vout[1].var, vout[2].var);

                // Interpolate varyings divided by w, then divide by invW_px
                // Varyings var_over_w_px =
                //    w0 * vout[0].var_over_w +
                //    w1 * vout[1].var_over_w +
                //    w2 * vout[2].var_over_w;
                // Varyings varying_px = var_over_w_px * (1.0f / invW_px);

                // Depth (if needed; same trick)
                MathType depth_px = w0 * vout[0].depth +
                                    w1 * vout[1].depth +
                                    w2 * vout[2].depth;
                // Depth test could go here

                linalg::Vec4<MathType> gl_FragCoord;
                gl_FragCoord(0) = static_cast<MathType>(x) + MathType(RenderConstants::PIXEL_CENTER_OFFSET);
                gl_FragCoord(1) = static_cast<MathType>(y) + MathType(RenderConstants::PIXEL_CENTER_OFFSET);
                gl_FragCoord(2) = depth_px;
                gl_FragCoord(3) = inv_invW_px;

                derived_().fragment_shader(inside, gl_FragCoord, varying_px, textures);

                // advance to x+1
                // eAB += eAB_dx;
                // eBC += eBC_dx;
                // eCA += eCA_dx;

                // advance to y+1: add dy increments and reset x terms
                /*
                if (x == x1 - 1)
                {
                    eAB_row += eAB_dy;
                    eBC_row += eBC_dy;
                    eCA_row += eCA_dy;

                    eAB = eAB_row;
                    eBC = eBC_row;
                    eCA = eCA_row;
                }
                */
            }

            // eAB_row += eAB_dy;
            // eBC_row += eBC_dy;
            // eCA_row += eCA_dy;
        }

        derived_().write_cache(y0, y1, x0, x1, textures);
    }

    // Derived derived_() { return static_cast<Derived &>(*this); }

    Derived &derived_() { return *static_cast<Derived *>(this); }
    const Derived &derived_() const { return *static_cast<const Derived *>(this); }

    linalg::Mat4<MathType> opencv2opengl_;
};

template <typename MathType, typename OutType, class Mesh, template <class> class Texture>
class GouraudRendererBase
    : public RendererBase<MathType, GouraudRendererBase<MathType, OutType, Mesh, Texture>>
{
public:
    // struct Buffers
    //{
    //     const Buffer<MathType> &pos_buffer;
    //     const Buffer<unsigned int> &ebo_buffer;
    // };

    struct Textures
    {
        Texture<linalg::Vec3<OutType>> &out_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec3<MathType> normal;
    };

    struct Varyings
    {
        linalg::Vec3<OutType> vColor;
    };

    GouraudRendererBase() = default;
    ~GouraudRendererBase() = default;

    void Render(const Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                const linalg::Vec3<MathType> &light_pos,
                const linalg::Vec3<MathType> &light_color,
                const linalg::Vec3<MathType> &ambient_reflectance,
                const linalg::Vec3<MathType> &diffuse_reflectance,
                const linalg::Vec3<MathType> &specular_reflectance,
                const MathType shininess,
                const linalg::Vec3<MathType> &ambient_light,
                unsigned int out_lvl,
                Texture<linalg::Vec3<OutType>> &out_texture)
    {
        out_texture.fill(out_lvl, out_texture.nodata());

        linalg::SE3<MathType> cam2world = pose.inverse();

        // model already in world space
        uModel_ = linalg::Mat4<MathType>::Identity();
        uView_ = this->opencv2opengl_ * pose.matrix();
        uProjection_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE);
        uNormalMatrix_ = linalg::Mat3<MathType>::Identity(); // linalg::Mat3<MathType>(uModel_).inverse().transpose();

        uLightPos_ = light_pos;
        uViewPos_ = cam2world.translation(); // camera position in world space

        uKa_ = ambient_reflectance;
        uKd_ = diffuse_reflectance;
        uKs_ = specular_reflectance;
        uShininess_ = shininess;
        uLightColor_ = light_color;
        uAmbientLight_ = ambient_light;

        out_lvl_ = out_lvl;

        // out_texture_ = &out_texture;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        // Buffers buffers{mesh.pos_buffer_, mesh.ebo_buffer_};
        Textures textures{out_texture};

        RendererBase<MathType, GouraudRendererBase>::Render(viewport, mesh, textures);
    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 2];

        vertexdata.normal(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.nor_offset_ + 0];
        vertexdata.normal(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.nor_offset_ + 1];
        vertexdata.normal(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.nor_offset_ + 2];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.vColor =
            (w0 * varying_px0.vColor +
             w1 * varying_px1.vColor +
             w2 * varying_px2.vColor);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        // Transform to world space
        linalg::Vec3<MathType> fragPos = linalg::Vec3<MathType>(uModel_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1)));
        linalg::Vec3<MathType> N = (uNormalMatrix_ * vertexdata.normal).normalized();

        // Lighting vectors
        linalg::Vec3<MathType> L = (uLightPos_ - fragPos).normalized();
        linalg::Vec3<MathType> V = (uViewPos_ - fragPos).normalized();
        MathType n_dot_l = N.dot(L);
        linalg::Vec3<MathType> R = L - MathType(2) * n_dot_l * N; // opengls reflect

        // Phong reflectance model (computed per-vertex)
        MathType NdotL = max(n_dot_l, MathType(0));
        MathType spec = 0.0;
        if (NdotL > 0.0)
        {
            spec = pow(max(V.dot(R), MathType(0)), uShininess_);
        }

        linalg::Vec3<MathType> ambient = uAmbientLight_ * uKa_;
        linalg::Vec3<MathType> diffuse = uLightColor_ * uKd_ * NdotL;
        linalg::Vec3<MathType> specular = uLightColor_ * uKs_ * spec;

        outVarying.vColor = ambient + diffuse + specular;

        gl_Position = uProjection_ * uView_ * linalg::Vec4<MathType>(fragPos, MathType(1));
    }

    void fragment_shader(bool inside,
                         const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        // std::cout << "calling fragment shader " << std::endl;
        if (!inside)
            return;

        textures.out_texture.set_texel_(in_varying.vColor, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

    void read_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

    void write_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

    linalg::Mat4<MathType> uModel_;        // model to world space
    linalg::Mat4<MathType> uView_;         // world space to camera space
    linalg::Mat4<MathType> uProjection_;   // camera space to clip space
    linalg::Mat3<MathType> uNormalMatrix_; // transpose(inverse(mat3(uModel))) computed on CPU

    linalg::Vec3<MathType> uLightPos_; // world space
    linalg::Vec3<MathType> uViewPos_;  // camera position in world space

    // Material and light
    linalg::Vec3<MathType> uKa_;           // ambient reflectance (rgb)
    linalg::Vec3<MathType> uKd_;           // diffuse reflectance (rgb)
    linalg::Vec3<MathType> uKs_;           // specular reflectance (rgb)
    MathType uShininess_;                  // specular exponent
    linalg::Vec3<MathType> uLightColor_;   // light color/intensity (rgb)
    linalg::Vec3<MathType> uAmbientLight_; // ambient light (rgb)

    unsigned int out_lvl_;
};

/*
template <typename MathType, typename DepthType, class Mesh, template <class> class Texture>
class PhongRendererBase
    : public RendererBase<MathType, DepthRendererBase<MathType, DepthType, Mesh, Texture>>
{
public:
    // struct Buffers
    //{
    //     const Buffer<MathType> &pos_buffer;
    //     const Buffer<unsigned int> &ebo_buffer;
    // };

    struct Textures
    {
        Texture<DepthType> &out_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec3<MathType> normal;
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        float depth;
    };

    PhongRendererBase() = default;
    ~PhongRendererBase() = default;

    void Render(const Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                unsigned int out_lvl,
                Texture<DepthType> &out_texture)
    {
        out_texture.fill(out_lvl, out_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_ * pose.matrix();
        out_lvl_ = out_lvl;

        // out_texture_ = &out_texture;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        // Buffers buffers{mesh.pos_buffer_, mesh.ebo_buffer_};
        Textures textures{out_texture};

        RendererBase<MathType, DepthRendererBase>::Render(viewport, mesh, textures);
    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.pos_buffer_[vertexid * 3 + 0];
        vertexdata.vertex(1) = mesh.pos_buffer_[vertexid * 3 + 1];
        vertexdata.vertex(2) = mesh.pos_buffer_[vertexid * 3 + 2];

        vertexdata.normal(0) = mesh.nor_buffer_[vertexid * 3 + 0];
        vertexdata.normal(1) = mesh.nor_buffer_[vertexid * 3 + 1];
        vertexdata.normal(2) = mesh.nor_buffer_[vertexid * 3 + 2];

        vertexdata.texcoord(0) = mesh.tex_buffer_[vertexid * 2 + 0];
        vertexdata.texcoord(1) = mesh.tex_buffer_[vertexid * 2 + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.depth =
            (w0 * varying_px0.depth +
             w1 * varying_px1.depth +
             w2 * varying_px2.depth);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        // #pragma HLS INLINE
        //  std::cout << "calling vertex shader " << std::endl;
        gl_Position = t_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        outVarying.depth = vertexdata.vertex(2);
    }

    void fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        // std::cout << "calling fragment shader " << std::endl;
        textures.out_texture.set_texel_(in_varying.depth, int(gl_FragCoord(1)), int(gl_FragCoord(0)), out_lvl_);
    }

    linalg::Mat4<MathType> t_matrix_;
    unsigned int out_lvl_;
};
*/
// -----------------------------------------------------------------------------
// DepthRenderer
//   Evout[0].screen(0)mple derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

template <typename MathType, typename DepthType, class Mesh, template <class> class Texture>
class DepthRendererBase
    : public RendererBase<MathType, DepthRendererBase<MathType, DepthType, Mesh, Texture>>
{
public:
    // struct Buffers
    //{
    //     const Buffer<MathType> &pos_buffer;
    //     const Buffer<unsigned int> &ebo_buffer;
    // };

    struct Textures
    {
        Texture<DepthType> &out_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
    };

    struct Varyings
    {
        MathType depth;
    };

    DepthRendererBase() = default;
    ~DepthRendererBase() = default;

    void Render(const Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                unsigned int out_lvl,
                Texture<DepthType> &out_texture)
    {
        out_texture.fill(out_lvl, out_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_ * pose.matrix();
        out_lvl_ = out_lvl;

        // out_texture_ = &out_texture;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        // Buffers buffers{mesh.pos_buffer_, mesh.ebo_buffer_};
        Textures textures{out_texture};

        RendererBase<MathType, DepthRendererBase>::Render(viewport, mesh, textures);
    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;
        unsigned int base = vertexid * mesh.stride_ + mesh.pos_offset_;
        vertexdata.vertex(0) = mesh.vertex_buffer_[base + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[base + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[base + 2];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.depth =
            (w0 * varying_px0.depth +
             w1 * varying_px1.depth +
             w2 * varying_px2.depth);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        // #pragma HLS INLINE
        //  std::cout << "calling vertex shader " << std::endl;
        gl_Position = t_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        outVarying.depth = gl_Position(2);
    }

    void fragment_shader(bool inside,
                         const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        // std::cout << "calling fragment shader " << std::endl;
        // MathType depth = (far_plane - near_plane)*gl_FragCoord(2) + near_plane;

        if (!inside)
            return;

        MathType depth = in_varying.depth;
        // DepthType depth_old = textures.out_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);

        // if (depth_old == textures.out_texture.nodata() || depth_old < depth)
        //{
        textures.out_texture.set_texel_(depth, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        //}

        /*
         int y = int(gl_FragCoord(1));
         int x = int(gl_FragCoord(0));
         int add = ((y - cache_y0_) << 5) + (x - cache_x0_);

         MathType depth = in_varying.depth;
         DepthType depth_old = cache_[add];
         if (inside && (depth_old == textures.out_texture.nodata() || depth_old < depth))
         {
             cache_[add] = depth;
         }
         */
    }

    void read_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
        /*
        cache_y0_ = y0;
        cache_y1_ = y1;
        cache_x0_ = x0;
        cache_x1_ = x1;

    depthrenderer_read_cache_y_loop:
        for (int y = 0; y < y1 - y0; ++y)
        {
#pragma HLS loop_tripcount min = 15 max = 15 avg = 15

        depthrenderer_read_cache_x_loop:
            for (int x = 0; x < x1 - x0; ++x)
            {
#pragma HLS loop_tripcount min = 20 max = 20 avg = 20

                DepthType data = textures.out_texture.texel_(y + y0, x + x0, out_lvl_);
                // cache_[y * 32 + x] = data;
                cache_[(y << 5) + x] = data;
            }
        }
            */
    }

    void write_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
        /*
    depthrenderer_write_cache_y_loop:
        for (int y = 0; y < y1 - y0; ++y)
        {
#pragma HLS loop_tripcount min = 15 max = 15 avg = 15

        depthrenderer_write_cache_x_loop:
            for (int x = 0; x < x1 - x0; ++x)
            {
#pragma HLS loop_tripcount min = 20 max = 20 avg = 20

                // DepthType data = cache_[y * 32 + x];
                DepthType data = cache_[(y << 5) + x];
                textures.out_texture.set_texel_(data, y + y0, x + x0, out_lvl_);
            }
        }
            */
    }

    linalg::Mat4<MathType> t_matrix_;
    DepthType cache_[32 * 32];
    int cache_y0_;
    int cache_y1_;
    int cache_x0_;
    int cache_x1_;
    unsigned int out_lvl_;
};

// -----------------------------------------------------------------------------
// ImageRendererBase
//   Another evout[0].screen(0)mple derived class that might output color
// -----------------------------------------------------------------------------

template <typename MathType, typename ImageType, class Mesh, template <class> class DiffuseTexture, template <class> class DepthTexture, template <class> class OutTexture>
class ImageRendererBase
    : public RendererBase<MathType, ImageRendererBase<MathType, ImageType, Mesh, DiffuseTexture, DepthTexture, OutTexture>>
{
public:
    // struct Buffers
    //{
    //     const Buffer<MathType> &pos_buffer;
    //     const Buffer<MathType> &tex_buffer;
    //     const Buffer<unsigned int> &ebo_buffer;
    // };

    struct Textures
    {
        DepthTexture<MathType> &depth_texture;
        DiffuseTexture<ImageType> &in_texture;
        OutTexture<ImageType> &out_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
    };

    ImageRendererBase() = default;
    ~ImageRendererBase() = default;

    void Render(Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                DepthTexture<MathType> &depth_texture,
                OutTexture<ImageType> &out_texture)
    {
        // #pragma HLS inline

        depth_texture.fill(out_lvl, depth_texture.nodata());
        out_texture.fill(out_lvl, out_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) *
                    this->opencv2opengl_ *
                    pose.matrix();
        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        // in_texture_ = &in_texture;
        // out_texture_ = &out_texture;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        // Buffers buffers{mesh.pos_buffer_, mesh.tex_buffer_, mesh.ebo_buffer_};
        Textures textures{depth_texture, mesh.diffuse_, out_texture};

        RendererBase<MathType, ImageRendererBase>::Render(viewport, mesh, textures);
    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        // #pragma HLS inline

        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        // #pragma HLS inline

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
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
#pragma HLS inline

        gl_Position = t_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        outVarying.texcoord = vertexdata.texcoord;
    }

    void fragment_shader(bool inside,
                         const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
#pragma HLS inline

        if (!inside)
            return;

        int y = int(gl_FragCoord(1));
        int x = int(gl_FragCoord(0));
        int add = ((y - cache_y0_) << 5) + (x - cache_x0_);

        MathType depth = gl_FragCoord(2);
        // MathType depth_old = textures.depth_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);

        MathType depth_old = depth_cache_[add];

        if (depth_old == textures.depth_texture.nodata() || depth < depth_old)
        {
            MathType pix = sample<MathType, DiffuseTexture<ImageType>>(textures.in_texture, in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
            // linalg::Vec2<MathType> screen_texcoord(in_varying.texcoord(0)*textures.in_texture.width(in_lvl_), in_varying.texcoord(1) * textures.in_texture.height(in_lvl_));
            // MathType pix = textures.in_texture.texel_(screen_texcoord(1), screen_texcoord(0), in_lvl_);
            textures.out_texture.set_texel_(pix, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
            // textures.depth_texture.set_texel_(depth, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
            depth_cache_[add] = depth;
        }
    }

    void read_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
#pragma HLS inline

        cache_y0_ = y0;
        cache_y1_ = y1;
        cache_x0_ = x0;
        cache_x1_ = x1;

        unsigned int w = textures.depth_texture.width(out_lvl_);
        unsigned int h = textures.depth_texture.height(out_lvl_);
        MathType *depth_data = textures.depth_texture.data(out_lvl_);

    imagerenderer_read_cache_y_loop:
        for (int y = 0; y < y1 - y0; ++y)
        {
#pragma HLS loop_tripcount min = 15 max = 15 avg = 15

        imagerenderer_read_cache_x_loop:
            for (int x = 0; x < x1 - x0; ++x)
            {
#pragma HLS loop_tripcount min = 20 max = 20 avg = 20
#pragma HLS PIPELINE II = 1

                //MathType data = textures.depth_texture.texel_(y + y0, x + x0, out_lvl_);
                MathType data = depth_data[(y + y0)*w + x + x0];
                // depth_cache_[y * 32 + x] = data;
                depth_cache_[(y << 5) + x] = data;
            }
        }
    }

    void write_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
#pragma HLS inline

    imagerenderer_write_cache_y_loop:
        for (int y = 0; y < y1 - y0; ++y)
        {
#pragma HLS loop_tripcount min = 15 max = 15 avg = 15

        imagerenderer_write_cache_x_loop:
            for (int x = 0; x < x1 - x0; ++x)
            {
#pragma HLS loop_tripcount min = 20 max = 20 avg = 20
#pragma HLS PIPELINE II = 1

                // DepthType data = cache_[y * 32 + x];
                MathType data = depth_cache_[(y << 5) + x];
                textures.depth_texture.set_texel_(data, y + y0, x + x0, out_lvl_);
            }
        }
    }

    linalg::Mat4<MathType> t_matrix_;
    unsigned int in_lvl_;
    unsigned int out_lvl_;
    MathType depth_cache_[32 * 32];
    unsigned int cache_y0_;
    unsigned int cache_y1_;
    unsigned int cache_x0_;
    unsigned int cache_x1_;
};

template <typename MathType, typename ImageType, typename ErrorType, class Mesh, template <class> class Texture>
class ResidualRendererBase
    : public RendererBase<MathType, ResidualRendererBase<MathType, ImageType, ErrorType, Mesh, Texture>>
{
public:
    // struct Buffers
    //{
    //     const Buffer<MathType> &pos_buffer;
    //     const Buffer<MathType> &tex_buffer;
    //     const Buffer<unsigned int> &ebo_buffer;
    // };

    struct Textures
    {
        Texture<ImageType> &kf_texture;
        Texture<ImageType> &f_texture;
        Texture<ErrorType> &r_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
    };

    ResidualRendererBase() = default;
    ~ResidualRendererBase() = default;

    void Render(const Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                Texture<ImageType> &kf_texture,
                Texture<ImageType> &f_texture,
                Texture<ErrorType> &r_texture)
    {
        r_texture.fill(out_lvl, r_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) *
                    this->opencv2opengl_ * pose.matrix();
        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        // kf_texture_ = &kf_texture;
        // f_texture_ = &f_texture;
        // r_texture_ = &r_texture;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        // Buffers buffers{mesh.pos_buffer_, mesh.tex_buffer_, mesh.ebo_buffer_};
        Textures textures{kf_texture, f_texture, r_texture};

        RendererBase<MathType, ResidualRendererBase>::Render(viewport, mesh, textures);
    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
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
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        gl_Position = t_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        outVarying.texcoord = vertexdata.texcoord;
    }

    void fragment_shader(bool inside,
                         const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        if (!inside)
            return;

        int width = textures.kf_texture.width(out_lvl_);
        int height = textures.kf_texture.height(out_lvl_);

        linalg::Vec2<MathType> screen_texcoord(gl_FragCoord(0) / MathType(width), gl_FragCoord(1) / MathType(height));

        ImageType kf = sample<ImageType, Texture<ImageType>>(textures.kf_texture, in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        ImageType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // float f = f_texture_->sample_(screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);

        if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata())
            return;

        ErrorType e = ErrorType(f) - ErrorType(kf);
        textures.r_texture.set_texel_(e, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

    void read_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

    void write_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

private:
    linalg::Mat4<MathType> t_matrix_;
    int in_lvl_;
    int out_lvl_;
    // const TextureCPU<float> *kf_texture_;
    // const TextureCPU<float> *f_texture_;
    // TextureCPU<float> *r_texture_;
};

template <typename MathType, typename ImageType, typename ErrorType, class Mesh, template <class> class Texture>
class L2RendererBase
    : public RendererBase<MathType, L2RendererBase<MathType, ImageType, ErrorType, Mesh, Texture>>
{
public:
    // struct Buffers
    //{
    //     const Buffer<MathType> &pos_buffer;
    //     const Buffer<MathType> &tex_buffer;
    //     const Buffer<unsigned int> &ebo_buffer;
    // };

    struct Textures
    {
        Texture<ImageType> &kf_texture;
        Texture<ImageType> &f_texture;
        Texture<ErrorType> &r_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
    };

    L2RendererBase() = default;
    ~L2RendererBase() = default;

    void Render(Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                Texture<ImageType> &f_texture,
                Texture<ErrorType> &r_texture)
    {
        r_texture.fill(out_lvl, r_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) *
                    this->opencv2opengl_ *
                    pose.matrix();
        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        // kf_texture_ = &kf_texture;
        // f_texture_ = &f_texture;
        // r_texture_ = &r_texture;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        // Buffers buffers{mesh.pos_buffer_, mesh.tex_buffer_, mesh.ebo_buffer_};
        Textures textures{mesh.diffuse_, f_texture, r_texture};

        RendererBase<MathType, L2RendererBase>::Render(viewport, mesh, textures);
    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
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
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        gl_Position = t_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        outVarying.texcoord = vertexdata.texcoord;
    }

    void fragment_shader(bool inside,
                         const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        if (!inside)
            return;

        unsigned int width = textures.kf_texture.width(out_lvl_);
        unsigned int height = textures.kf_texture.height(out_lvl_);

        linalg::Vec2<MathType> screen_texcoord(gl_FragCoord(0) / MathType(width), gl_FragCoord(1) / MathType(height));

        ImageType kf = sample<ImageType, Texture<ImageType>>(textures.kf_texture, in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        ImageType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // MathType f = sample<T, Texture<MathType>>(textures.f_texture, screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);

        if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata())
            return;

        ErrorType e = ErrorType(f) - ErrorType(kf);
        textures.r_texture.set_texel_(e * e, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

    void read_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

    void write_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

private:
    linalg::Mat4<MathType> t_matrix_;
    int in_lvl_;
    int out_lvl_;
    // const TextureCPU<float> *kf_texture_;
    // const TextureCPU<float> *f_texture_;
    // TextureCPU<float> *r_texture_;
};

template <typename MathType, typename ImageType, typename DType, class Mesh, template <class> class Texture>
class DIDxyRendererBase
    : public RendererBase<MathType, DIDxyRendererBase<MathType, ImageType, DType, Mesh, Texture>>
{
public:
    // struct Buffers
    //{
    //     const Buffer<MathType> &tex_buffer;
    //     const Buffer<unsigned int> &ebo_buffer;
    // };

    struct Textures
    {
        Texture<ImageType> &in_texture;
        Texture<linalg::Vec3<DType>> &out_texture;
    };

    struct VertexData
    {
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
    };

    DIDxyRendererBase() = default;
    ~DIDxyRendererBase() = default;

    void Render(const Mesh &mesh,
                int in_lvl,
                int out_lvl,
                Texture<ImageType> &in_texture,
                Texture<linalg::Vec3<DType>> &out_texture)
    {
        out_texture.fill(out_lvl, out_texture.nodata());

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        // in_texture_ = &in_texture;
        // out_texture_ = &out_texture;

        const int W = static_cast<int>(out_texture.width(out_lvl));
        const int H = static_cast<int>(out_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        // Buffers buffers{mesh.tex_buffer_, mesh.ebo_buffer_};
        Textures textures{in_texture, out_texture};

        RendererBase<MathType, DIDxyRendererBase>::Render(viewport, mesh, textures);
    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.texcoord(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
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
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        // gl_Position = (view_matrix * pose_matrix) * linalg::Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = linalg::Vec4<MathType>(2.0 * vertexdata.texcoord(0) - 1.0, 2.0 * vertexdata.texcoord(1) - 1.0, 0.0, 1.0);
        outVarying.texcoord = vertexdata.texcoord;
    }

    void fragment_shader(bool inside,
                         const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        // outFragment = inVarying;
        if (!inside)
            return;

        unsigned int height = textures.in_texture.height(out_lvl_);
        unsigned int width = textures.in_texture.width(out_lvl_);
        ImageType nodata = textures.in_texture.nodata();

        int x = int(in_varying.texcoord(0) * (width - 1));
        int y = int(in_varying.texcoord(1) * (height - 1));
        int x_p = x + 1;
        int x_m = x - 1;
        int y_p = y + 1;
        int y_m = y - 1;

        if (x_p >= width || x_m < 0 || y_p >= height || y_m < 0)
        {
            // No need to explicitly set to nodata, it is already in the background color
            // outFragment(0) = 0.0f;
            // outFragment(1) = 0.0f;
            // outFragment(2) = 0.0f;
            return;
        }

        ImageType f = textures.in_texture.texel_(y, x, out_lvl_);
        ImageType f_y_p = textures.in_texture.texel_(y_p, x, out_lvl_);
        ImageType f_y_m = textures.in_texture.texel_(y_m, x, out_lvl_);
        ImageType f_x_p = textures.in_texture.texel_(y, x_p, out_lvl_);
        ImageType f_x_m = textures.in_texture.texel_(y, x_m, out_lvl_);

        if (f_x_p == nodata || f_x_m == nodata ||
            f_y_p == nodata || f_y_m == nodata || f == nodata)
        {
            // No need to explicitly set to nodata, it is already in the background color
            // outFragment(0) = 0.0f;
            // outFragment(1) = 0.0f;
            // outFragment(2) = 0.0f;
            return;
        }

        linalg::Vec3<MathType> out_fragment;
        out_fragment(0) = (f_x_p - f_x_m) / 2.0f;
        out_fragment(1) = (f_y_p - f_y_m) / 2.0f;
        out_fragment(2) = 0.0; // f; // save the projected frame for later processing

        textures.out_texture.set_texel_(out_fragment, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

    void read_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

    void write_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

private:
    int in_lvl_;
    int out_lvl_;
    // const TextureCPU<float> *in_texture_;
    // TextureCPU<Vec3> *out_texture_;
};

template <typename MathType, typename ImageType, typename DType, typename ErrorType, class Mesh, template <class> class Texture>
class JPoseRendererBase
    : public RendererBase<MathType, JPoseRendererBase<MathType, ImageType, DType, ErrorType, Mesh, Texture>>
{
public:
    // struct Buffers
    //{
    //     const Buffer<MathType> &pos_buffer;
    //     const Buffer<MathType> &tex_buffer;
    //     const Buffer<unsigned int> &ebo_buffer;
    // };

    struct Textures
    {
        Texture<ImageType> &kf_texture;
        Texture<ImageType> &f_texture;
        Texture<linalg::Vec3<DType>> &dfdxy_texture;
        Texture<linalg::Vec3<DType>> &jtra_texture;
        Texture<linalg::Vec3<DType>> &jrot_texture;
        Texture<ErrorType> &r_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
        linalg::Vec3<MathType> f_ver;
    };

    JPoseRendererBase() = default;
    ~JPoseRendererBase() = default;

    void Render(Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                Texture<ImageType> &f_texture,
                Texture<linalg::Vec3<DType>> &dfdxy_texture,
                Texture<linalg::Vec3<DType>> &jtra_texture,
                Texture<linalg::Vec3<DType>> &jrot_texture,
                Texture<ErrorType> &r_texture)
    {
        jtra_texture.fill(out_lvl, jtra_texture.nodata());
        jrot_texture.fill(out_lvl, jrot_texture.nodata());
        r_texture.fill(out_lvl, r_texture.nodata());

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        fx_ = cam.GetParams()(0);
        fy_ = cam.GetParams()(1);
        view_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_;
        pose_matrix_ = pose.matrix();
        // kf_texture_ = &kf_texture;
        // f_texture_ = &f_texture;
        // dfdxy_texture_ = &dfdxy_texture;
        // jtra_texture_ = &jtra_texture;
        // jrot_texture_ = &jrot_texture;
        // r_texture_ = &r_texture;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        // Buffers buffers{mesh.pos_buffer_, mesh.tex_buffer_, mesh.ebo_buffer_};
        Textures textures{mesh.diffuse_, f_texture, dfdxy_texture, jtra_texture, jrot_texture, r_texture};

        RendererBase<MathType, JPoseRendererBase>::Render(viewport, mesh, textures);
    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord +
             w1 * varying_px1.texcoord +
             w2 * varying_px2.texcoord);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        return var_over_w_px;
    }
    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        linalg::Vec4<MathType> f_ver = pose_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        gl_Position = view_matrix_ * f_ver;

        outVarying.f_ver = linalg::Vec3<MathType>(f_ver);
        outVarying.texcoord = vertexdata.texcoord;
    }

    void fragment_shader(bool inside,
                         const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        if (!inside)
            return;

        unsigned int width = textures.kf_texture.width(out_lvl_);
        unsigned int height = textures.kf_texture.height(out_lvl_);

        linalg::Vec2<MathType> screen_texcoord(gl_FragCoord(0) / MathType(width), gl_FragCoord(1) / MathType(height));

        linalg::Vec3<MathType> f_ver = in_varying.f_ver;
        linalg::Vec2<MathType> texcoord = in_varying.texcoord;

        ImageType kf = sample<ImageType, Texture<ImageType>>(textures.kf_texture, texcoord(1), texcoord(0), in_lvl_);
        ImageType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        linalg::Vec3<DType> f_der = textures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // float f = sample<T, Texture<MathType>>(textures.f_texture, screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);
        // linalg::Vec3<MathType>f_der = sample<Vec3, Texture<Vec3>>(textures.dfdxy_texture, screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);

        if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata() || f_der == textures.dfdxy_texture.nodata())
            return;

        ErrorType r = ErrorType(f) - ErrorType(kf);

        MathType v0 = MathType(f_der(0)) * fx_ * width / f_ver(2);
        MathType v1 = MathType(f_der(1)) * fy_ * height / f_ver(2);
        MathType v2 = -(v0 * f_ver(0) + v1 * f_ver(1)) / f_ver(2);

        linalg::Vec3<MathType> d_f_i_d_tra = linalg::Vec3<MathType>(v0, v1, v2);
        linalg::Vec3<MathType> d_f_i_d_rot = linalg::Vec3<MathType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        textures.jtra_texture.set_texel_(d_f_i_d_tra, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.jrot_texture.set_texel_(d_f_i_d_rot, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.r_texture.set_texel_(r, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

    void read_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

    void write_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

private:
    int in_lvl_;
    int out_lvl_;
    MathType fx_;
    MathType fy_;
    linalg::Mat4<MathType> view_matrix_;
    linalg::Mat4<MathType> pose_matrix_;
    // const TextureCPU<float> *kf_texture_;
    // const TextureCPU<float> *f_texture_;
    // const TextureCPU<Vec3> *dfdxy_texture_;
    // TextureCPU<Vec3> *jtra_texture_;
    // TextureCPU<Vec3> *jrot_texture_;
    // TextureCPU<float> *r_texture_;
};

template <typename MathType, typename ImageType, typename DType, typename IdType, typename ErrorType, class Mesh, template <class> class Texture>
class JMapRendererBase
    : public RendererBase<MathType, JMapRendererBase<MathType, ImageType, DType, IdType, ErrorType, Mesh, Texture>>
{
public:
    // struct Buffers
    //{
    //     const Buffer<MathType> &pos_buffer;
    //     const Buffer<MathType> &tex_buffer;
    //     const Buffer<unsigned int> &ebo_buffer;
    // };

    struct Textures
    {
        Texture<ImageType> &kf_texture;
        Texture<ImageType> &f_texture;
        Texture<linalg::Vec3<DType>> &dfdxy_texture;
        Texture<linalg::Vec3<DType>> &jmap_texture;
        Texture<linalg::Vec3<IdType>> &pids_texture;
        Texture<ErrorType> &r_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec2<MathType> texcoord;
    };
    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
        linalg::Vec3<MathType> f_ver;
        linalg::Vec3<MathType> kf_ray;
        MathType depth;
        linalg::Vec3<MathType> baricentric;
        unsigned int vertexId;
        linalg::Vec3<int> pids;
    };

    JMapRendererBase() = default;
    ~JMapRendererBase() = default;

    void Render(Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                Texture<ImageType> &f_texture,
                Texture<linalg::Vec3<DType>> &dfdxy_texture,
                Texture<linalg::Vec3<DType>> &jmap_texture,
                Texture<linalg::Vec3<IdType>> &pids_texture,
                Texture<ErrorType> &r_texture)
    {
        jmap_texture.fill(out_lvl, jmap_texture.nodata());
        pids_texture.fill(out_lvl, pids_texture.nodata());
        r_texture.fill(out_lvl, r_texture.nodata());

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        fx_ = cam.GetParams()(0);
        fy_ = cam.GetParams()(1);
        view_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_;
        pose_matrix_ = pose.matrix();
        // kf_texture_ = &kf_texture;
        // f_texture_ = &f_texture;
        // dfdxy_texture_ = &dfdxy_texture;
        // jmap_texture_ = &jmap_texture;
        // pids_texture_ = &pids_texture;
        // r_texture_ = &r_texture;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        // Buffers buffers{mesh.pos_buffer_, mesh.tex_buffer_, mesh.ebo_buffer_};
        Textures textures{mesh.diffuse_, f_texture, dfdxy_texture, jmap_texture, pids_texture, r_texture};

        RendererBase<MathType, JMapRendererBase>::Render(viewport, mesh, textures);
    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord +
             w1 * varying_px1.texcoord +
             w2 * varying_px2.texcoord);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ray =
            (w0 * varying_px0.kf_ray +
             w1 * varying_px1.kf_ray +
             w2 * varying_px2.kf_ray);
        // var_over_w_px.barvout[2].screen(1)entric = linalg::Vec3<MathType>(w0 * invW0 * varying_px0.depth,
        //                                  w1 * invW1 * varying_px1.depth,
        //                                  w2 * invW2 * varying_px2.depth) *
        //                             (1.0f / invW_px);
        var_over_w_px.baricentric = linalg::Vec3<MathType>(w0,
                                                           w1,
                                                           w2);
        var_over_w_px.pids = linalg::Vec3<int>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        linalg::Vec4<MathType> f_ver = pose_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        gl_Position = view_matrix_ * f_ver;

        linalg::Vec3<MathType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), MathType(1));
        linalg::Vec4<MathType> d_f_ver_d_kf_depth_ = pose_matrix_ * linalg::Vec4<MathType>(kf_ray, MathType(0));
        linalg::Vec3<MathType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = linalg::Vec3<MathType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
        outVarying.texcoord = vertexdata.texcoord;
    }

    void fragment_shader(bool inside,
                         const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        if (!inside)
            return;
        unsigned int width = textures.jmap_texture.width(out_lvl_);
        unsigned int height = textures.jmap_texture.height(out_lvl_);

        linalg::Vec2<MathType> screen_texcoord(gl_FragCoord(0) / MathType(width), gl_FragCoord(1) / MathType(height));

        linalg::Vec3<MathType> f_ver = in_varying.f_ver;
        linalg::Vec3<MathType> kf_ray = in_varying.kf_ray;
        linalg::Vec2<MathType> texcoord = in_varying.texcoord;
        linalg::Vec3<MathType> baricentric = in_varying.baricentric;
        linalg::Vec3<int> vertexid = in_varying.pids;

        ImageType kf = sample<ImageType, Texture<ImageType>>(textures.kf_texture, texcoord(1), texcoord(0), in_lvl_);
        ImageType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        linalg::Vec3<DType> f_der = textures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // float f = f_texture_->sample_(screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);
        // linalg::Vec3<MathType>f_der = dfdxy_texture_->sample_(screen_tevout[2].screen(0)oord(1), screen_tevout[2].screen(0)oord(0), in_lvl_);

        if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata() || f_der == textures.dfdxy_texture.nodata())
            return;

        ErrorType r = ErrorType(f) - ErrorType(kf);

        linalg::Vec3<MathType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = MathType(f_der(0)) * fx_ * width / f_ver(2);
        d_f_i_d_f_ver(1) = MathType(f_der(1)) * fy_ * height / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // linalg::Vec3<MathType>d_f_i_d_tra = linalg::Vec3<MathType>(v0, v1, v2);
        // linalg::Vec3<MathType>d_f_i_d_rot = linalg::Vec3<MathType>(-f_ver(2) * v1 + f_ver(1) * v2, f_ver(2) * v0 - f_ver(0) * v2, -f_ver(1) * v0 + f_ver(0) * v1);

        linalg::Vec3<MathType> d_f_ver_d_kf_depth = kf_ray; // kfTofPose.rotationMatrix() * kf_ray;
        MathType d_f_i_d_kf_depth = (d_f_i_d_f_ver.transpose() * d_f_ver_d_kf_depth)(0, 0);

        linalg::Vec3<MathType> d_depth_d_vert_depth = baricentric;

        linalg::Vec3<MathType> jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;
        linalg::Vec3<IdType> ids = linalg::Vec3<IdType>(vertexid(0), vertexid(1), vertexid(2));

        textures.jmap_texture.set_texel_(jac, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.pids_texture.set_texel_(ids, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.r_texture.set_texel_(r, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

    void read_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

    void write_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

private:
    int in_lvl_;
    int out_lvl_;
    MathType fx_;
    MathType fy_;
    linalg::Mat4<MathType> view_matrix_;
    linalg::Mat4<MathType> pose_matrix_;
    // const TextureCPU<float> *kf_texture_;
    // const TextureCPU<float> *f_texture_;
    // const TextureCPU<Vec3> *dfdxy_texture_;
    // TextureCPU<Vec3> *jmap_texture_;
    // TextureCPU<Vec3> *pids_texture_;
    // TextureCPU<float> *r_texture_;
};

template <typename MathType, typename ImageType, typename DepthType, typename DType, typename IdType, class Mesh, template <class> class Texture>
class DiffRendererBase
    : public RendererBase<MathType, DiffRendererBase<MathType, ImageType, DepthType, DType, IdType, Mesh, Texture>>
{
public:
    // struct Buffers
    // {
    //    const Buffer<MathType> &pos_buffer;
    //   const Buffer<MathType> &tex_buffer;
    //   const Buffer<unsigned int> &ebo_buffer;
    //};

    struct Textures
    {
        Texture<ImageType> &diffuse_texture;
        Texture<ImageType> &image_texture;
        Texture<DepthType> &depth_texture;
        Texture<linalg::Vec3<DType>> &jtra_texture;
        Texture<linalg::Vec3<DType>> &jrot_texture;
        Texture<linalg::Vec3<DType>> &jmap_texture;
        Texture<linalg::Vec3<IdType>> &pids_texture;
    };

    struct VertexData
    {
        linalg::Vec3<MathType> vertex;
        linalg::Vec2<MathType> texcoord;
    };

    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
        linalg::Vec3<MathType> f_ver;
        linalg::Vec3<MathType> kf_ray;
        MathType depth;
        linalg::Vec3<MathType> baricentric;
        unsigned int vertexId;
        linalg::Vec3<int> pids;
    };

    DiffRendererBase() = default;
    ~DiffRendererBase() = default;

    void Render(Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                Texture<ImageType> &image_texture,
                Texture<DepthType> &depth_texture,
                Texture<linalg::Vec3<DType>> &jtra_texture,
                Texture<linalg::Vec3<DType>> &jrot_texture,
                Texture<linalg::Vec3<DType>> &jmap_texture,
                Texture<linalg::Vec3<IdType>> &pids_texture)
    {
        image_texture.fill(out_lvl, image_texture.nodata());
        depth_texture.fill(out_lvl, depth_texture.nodata());
        jtra_texture.fill(out_lvl, jtra_texture.nodata());
        jrot_texture.fill(out_lvl, jrot_texture.nodata());
        jmap_texture.fill(out_lvl, jmap_texture.nodata());
        pids_texture.fill(out_lvl, pids_texture.nodata());

        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        fx_ = cam.GetParams()(0);
        fy_ = cam.GetParams()(1);
        view_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_;
        pose_matrix_ = pose.matrix();
        // kf_texture_ = &kf_texture;
        // f_texture_ = &f_texture;
        // dfdxy_texture_ = &dfdxy_texture;
        // jmap_texture_ = &jmap_texture;
        // pids_texture_ = &pids_texture;
        // r_texture_ = &r_texture;

        const int W = static_cast<int>(jtra_texture.width(out_lvl));
        const int H = static_cast<int>(jtra_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        // Buffers buffers{mesh.pos_buffer_, mesh.tex_buffer_, mesh.ebo_buffer_};
        Textures textures{mesh.diffuse_, image_texture, depth_texture, jtra_texture, jrot_texture, jmap_texture, pids_texture};

        RendererBase<MathType, DiffRendererBase>::Render(viewport, mesh, textures);
    }

    VertexData get_vertex_data(const Mesh &mesh, const unsigned int vertexid)
    {
        VertexData vertexdata;

        vertexdata.vertex(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 0];
        vertexdata.vertex(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 1];
        vertexdata.vertex(2) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.pos_offset_ + 2];

        vertexdata.texcoord(0) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 0];
        vertexdata.texcoord(1) = mesh.vertex_buffer_[vertexid * mesh.stride_ + mesh.tex_offset_ + 1];

        return vertexdata;
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord +
             w1 * varying_px1.texcoord +
             w2 * varying_px2.texcoord);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver +
             w1 * varying_px1.f_ver +
             w2 * varying_px2.f_ver);
        var_over_w_px.kf_ray =
            (w0 * varying_px0.kf_ray +
             w1 * varying_px1.kf_ray +
             w2 * varying_px2.kf_ray);
        var_over_w_px.baricentric = linalg::Vec3<MathType>(w0,
                                                           w1,
                                                           w2);

        var_over_w_px.pids = linalg::Vec3<int>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const VertexData &vertexdata,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        linalg::Vec4<MathType> f_ver = pose_matrix_ * linalg::Vec4<MathType>(vertexdata.vertex, MathType(1));
        gl_Position = view_matrix_ * f_ver;

        linalg::Vec3<MathType> kf_ray(vertexdata.vertex(0) / vertexdata.vertex(2), vertexdata.vertex(1) / vertexdata.vertex(2), MathType(1));
        linalg::Vec3<MathType> d_f_ver_d_kf_depth = linalg::Vec3<MathType>(pose_matrix_ * linalg::Vec4<MathType>(kf_ray, MathType(0)));
        // linalg::Vec3<MathType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = linalg::Vec3<MathType>(f_ver);
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = vertexdata.vertex(2);
        outVarying.vertexId = vertexid;
        outVarying.texcoord = vertexdata.texcoord;
    }

    void fragment_shader(bool inside,
                         const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        if (!inside)
            return;

        unsigned int width = textures.jmap_texture.width(out_lvl_);
        unsigned int height = textures.jmap_texture.height(out_lvl_);

        linalg::Vec3<MathType> f_ver = in_varying.f_ver;
        linalg::Vec3<MathType> kf_ray = in_varying.kf_ray;
        linalg::Vec2<MathType> texcoord = in_varying.texcoord;
        linalg::Vec3<MathType> baricentric = in_varying.baricentric;
        linalg::Vec3<int> vertexid = in_varying.pids;

        // MathType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        ImageType f = sample<ImageType, Texture<ImageType>>(textures.diffuse_texture, in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        if (f == textures.diffuse_texture.nodata())
            return;

        linalg::Vec3<DType> f_der = compute_didxy<DType, linalg::Vec3, Texture>(textures.diffuse_texture, in_varying.texcoord(1) * height, in_varying.texcoord(0) * width, in_lvl_);

        if (f_der(0) == textures.diffuse_texture.nodata() && f_der(1) == textures.diffuse_texture.nodata())
            return;

        linalg::Vec3<MathType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = f_der(0) * fx_ * width / f_ver(2);
        d_f_i_d_f_ver(1) = f_der(1) * fy_ * height / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // linalg::Vec3<MathType>d_f_i_d_tra = linalg::Vec3<MathType>(v0, v1, v2);
        linalg::Vec3<MathType> d_f_i_d_rot = linalg::Vec3<MathType>(-f_ver(2) * d_f_i_d_f_ver(1) + f_ver(1) * d_f_i_d_f_ver(2), f_ver(2) * d_f_i_d_f_ver(0) - f_ver(0) * d_f_i_d_f_ver(2), -f_ver(1) * d_f_i_d_f_ver(0) + f_ver(0) * d_f_i_d_f_ver(1));

        linalg::Vec3<MathType> d_f_ver_d_kf_depth = kf_ray; // kfTofPose.rotationMatrix() * kf_ray;
        MathType d_f_i_d_kf_depth = (d_f_i_d_f_ver.transpose() * d_f_ver_d_kf_depth)(0, 0);

        linalg::Vec3<MathType> d_depth_d_vert_depth = baricentric;

        linalg::Vec3<MathType> jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;
        linalg::Vec3<IdType> ids = linalg::Vec3<IdType>(vertexid(0), vertexid(1), vertexid(2));

        textures.image_texture.set_texel_(f, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.depth_texture.set_texel_(f_ver(2), gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.jtra_texture.set_texel_(d_f_i_d_f_ver, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.jrot_texture.set_texel_(d_f_i_d_rot, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.jmap_texture.set_texel_(jac, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.pids_texture.set_texel_(ids, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

    void read_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

    void write_cache(int y0, int y1, int x0, int x1, Textures &textures)
    {
    }

private:
    int in_lvl_;
    int out_lvl_;
    MathType fx_;
    MathType fy_;
    linalg::Mat4<MathType> view_matrix_;
    linalg::Mat4<MathType> pose_matrix_;
    // const TextureCPU<float> *kf_texture_;
    // const TextureCPU<float> *f_texture_;
    // const TextureCPU<Vec3> *dfdxy_texture_;
    // TextureCPU<Vec3> *jmap_texture_;
    // TextureCPU<Vec3> *pids_texture_;
    // TextureCPU<float> *r_texture_;
};
