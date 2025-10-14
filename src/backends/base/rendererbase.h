#pragma once

// #include <algorithm>
// #include <cmath>
// #include <cstdint>
#include "linalg/linalg.h"
#include "core/render_constants.h"
// #include "core/error_handling.h"

template <typename T>
inline T cross(const linalg::Vec2<T> &a, const linalg::Vec2<T> &b) { return a(0) * b(1) - a(1) * b(0); }

template <typename T>
inline T triangle_area(const linalg::Vec2<T> &p0, const linalg::Vec2<T> &p1, const linalg::Vec2<T> &p2) { return cross<T>(p1 - p0, p2 - p0); }

// Edge function E_ab(p) = (yb-ya)*px + (xa-xb)*py + (xb*ya - xa*yb)
template <typename T>
inline T edge_func(T ax, T ay, T bx, T by, T px, T py)
{
    return (by - ay) * px + (ax - bx) * py + (bx * ay - ax * by);
}

// Top-left test: returns true if edge is a "top" or "left" edge
template <typename T>
inline bool is_top_left(T ax, T ay, T bx, T by)
{
    return (ay == by) ? (bx < ax) : (ay < by);
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
        opencv2opengl_(2, 2) = -1.0; // flip Z like your original intent
    };
    // virtual ~RendererBase() = default;
    ~RendererBase() = default;

    template <typename Mesh, typename Textures>
    void Render(const Mesh &mesh,
                const BoundingBox<int> &viewport,
                Textures &textures)
    {
    // ---- Map mesh buffers (no copies) ----
    // auto pos = mesh.MapReadPositions(); // 3 floats/vertex
    // auto tex = mesh.MapReadTexcoords(); // 2 floats/vertex
    // auto wei = mesh.MapReadWeights();   // 1 float /vertex
    // auto idx = mesh.MapReadIndices();   // uint32_t indices

    // const auto pos = mesh.Positions(); // 3 floats/vertex
    // const auto tex = mesh.Texcoords(); // 2 floats/vertex
    // const auto wei = mesh.Weights();   // 1 float /vertex
    // const auto idx = mesh.Indices();   // uint32_t indices

    // Loop over triangles
    renderbase_triangle_loop:
        for (unsigned int i = 0; i + 2 < mesh.ebo_buffer_.size(); i += 3)
        {
#pragma HLS loop_tripcount min = 6144 max = 6144 avg = 6144

            const unsigned int i0 = mesh.ebo_buffer_[i + 0];
            const unsigned int i1 = mesh.ebo_buffer_[i + 1];
            const unsigned int i2 = mesh.ebo_buffer_[i + 2];

            linalg::Vec3<MathType> v[3];
            linalg::Vec2<MathType> uv[3];
            MathType wght[3];
            unsigned int id[3];

            id[0] = i0;
            id[1] = i1;
            id[2] = i2;

        renderbase_gather_loop:
            for (int k = 0; k < 3; ++k)
            {
                unsigned int vi = (k == 0 ? i0 : k == 1 ? i1
                                                        : i2);
                v[k](0) = mesh.pos_buffer_[vi * 3 + 0];
                v[k](1) = mesh.pos_buffer_[vi * 3 + 1];
                v[k](2) = mesh.pos_buffer_[vi * 3 + 2];
                uv[k](0) = mesh.tex_buffer_[vi * 2 + 0];
                uv[k](1) = mesh.tex_buffer_[vi * 2 + 1];
                wght[k] = mesh.wei_buffer_[vi];
            }

            this->draw_triangle_(v, uv, wght, id, viewport, textures);
        }
    }

protected:
    // Triangle rasterizer (top-left rule, perspective correct)
    template <typename Textures>
    void draw_triangle_(const linalg::Vec3<MathType> *verts,
                        const linalg::Vec2<MathType> *texcoords,
                        const MathType *weights,
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
            linalg::Vec4<MathType> gl_Position;
            typename Derived::Varyings varyings;
            derived_().vertex_shader(verts[i], texcoords[i], weights[i], vertexid[i], gl_Position, varyings);

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
        const MathType area = triangle_area<MathType>(vout[0].screen,
                                                      vout[1].screen,
                                                      vout[2].screen);
        // ErrorHandling::ValidateTriangleArea(area);
        //  if (area <= 0) return;            // enable to cull backfaces

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

        // Edge setup (top-left rule)
        const MathType xA = vout[0].screen(0), yA = vout[0].screen(1);
        const MathType xB = vout[1].screen(0), yB = vout[1].screen(1);
        const MathType xC = vout[2].screen(0), yC = vout[2].screen(1);

        // const MathType area2 = edge_func(xA, yA, xB, yB, xC, yC); // 2*area with sign
        // ErrorHandling::ValidateTriangleArea(area2);
        // ErrorHandling::ValidateNonZero(area2, "triangle area calculation");

        const MathType inv_area = MathType(1) / area;

        const bool tlAB = is_top_left(xA, yA, xB, yB);
        const bool tlBC = is_top_left(xB, yB, xC, yC);
        const bool tlCA = is_top_left(xC, yC, xA, yA);

        // Evaluate edge functions at top-left corner of each pixel (add +0.5)
        const MathType px0 = static_cast<MathType>(x0) + MathType(RenderConstants::PIXEL_CENTER_OFFSET);
        const MathType py0 = static_cast<MathType>(y0) + MathType(RenderConstants::PIXEL_CENTER_OFFSET);

        MathType eAB_row = edge_func(xA, yA, xB, yB, px0, py0);
        MathType eBC_row = edge_func(xB, yB, xC, yC, px0, py0);
        MathType eCA_row = edge_func(xC, yC, xA, yA, px0, py0);

        // Step increments when moving +1 in X or +1 in Y
        const MathType eAB_dx = (yB - yA);
        const MathType eAB_dy = (xA - xB);
        const MathType eBC_dx = (yC - yB);
        const MathType eBC_dy = (xB - xC);
        const MathType eCA_dx = (yA - yC);
        const MathType eCA_dy = (xC - xA);

    // Rasterize
    draw_triangle_raster_loop_y:
        for (int y = y0; y < y1; ++y)
        {
#pragma HLS loop_tripcount min = 10 max = 10 avg = 10

            MathType eAB = eAB_row;
            MathType eBC = eBC_row;
            MathType eCA = eCA_row;

        draw_triangle_raster_loop_x:
            for (int x = x0; x < x1; ++x)
            {
#pragma HLS loop_tripcount min = 10 max = 10 avg = 10

                // Top-left rule adjustments (include pixels on top/left edges)
                const bool inside =
                    (eAB > 0 || (eAB == 0 && tlAB)) &&
                    (eBC > 0 || (eBC == 0 && tlBC)) &&
                    (eCA > 0 || (eCA == 0 && tlCA));

                if (inside)
                {
                    // Barycentric weights normalized
                    const MathType w0 = eBC * inv_area;
                    const MathType w1 = eCA * inv_area;
                    const MathType w2 = eAB * inv_area;

                    // Perspective: 1/w at pixel
                    const MathType invW_px = w0 * vout[0].invW + w1 * vout[1].invW + w2 * vout[2].invW;

                    typename Derived::Varyings varying_px = derived_().interpolate_varyings(w0, w1, w2,
                                                                                            vout[0].invW, vout[1].invW, vout[2].invW,
                                                                                            invW_px,
                                                                                            vout[0].var, vout[1].var, vout[2].var);

                    // Interpolate varyings divided by w, then divide by invW_px
                    // Varyings var_over_w_px =
                    //    w0 * vout[0].var_over_w +
                    //    w1 * vout[1].var_over_w +
                    //    w2 * vout[2].var_over_w;
                    // Varyings varying_px = var_over_w_px * (1.0f / invW_px);

                    // Depth (if needed; same trick)
                    MathType depth_px = w0 * (vout[0].depth * vout[0].invW) +
                                        w1 * (vout[1].depth * vout[1].invW) +
                                        w2 * (vout[2].depth * vout[2].invW);
                    depth_px *= MathType(1) / invW_px;
                    // Depth test could go here

                    linalg::Vec4<MathType> gl_FragCoord;
                    gl_FragCoord(0) = static_cast<MathType>(x) + MathType(RenderConstants::PIXEL_CENTER_OFFSET);
                    gl_FragCoord(1) = static_cast<MathType>(y) + MathType(RenderConstants::PIXEL_CENTER_OFFSET);
                    gl_FragCoord(2) = depth_px;
                    gl_FragCoord(3) = MathType(1) / invW_px;

                    derived_().fragment_shader(gl_FragCoord, varying_px, textures);
                }

                // advance to x+1
                eAB += eAB_dx;
                eBC += eBC_dx;
                eCA += eCA_dx;
            }

            // next row y+1: add dy increments and reset x terms
            eAB_row += eAB_dy;
            eBC_row += eBC_dy;
            eCA_row += eCA_dy;
        }
    }

    // Derived derived_() { return static_cast<Derived &>(*this); }

    Derived &derived_() { return *static_cast<Derived *>(this); }
    const Derived &derived_() const { return *static_cast<const Derived *>(this); }

    linalg::Mat4<MathType> opencv2opengl_;
};

// -----------------------------------------------------------------------------
// DepthRenderer
//   Example derived renderer that outputs a "depth" or modifies Z
// -----------------------------------------------------------------------------

template <typename MathType, typename DepthType, class Mesh, template <class> class Texture>
class DepthRendererBase
    : public RendererBase<MathType, DepthRendererBase<MathType, DepthType, Mesh, Texture>>
{
public:
    struct Varyings
    {
        float depth;
    };

    struct Textures
    {
        Texture<DepthType> &out_texture;
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

        Textures textures{out_texture};

        RendererBase<MathType, DepthRendererBase>::Render(mesh, viewport, textures);
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const MathType invW0, const MathType invW1, const MathType invW2,
                                  const MathType invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.depth =
            (w0 * varying_px0.depth * invW0 +
             w1 * varying_px1.depth * invW1 +
             w2 * varying_px2.depth * invW2) *
            (MathType(1) / invW_px);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const linalg::Vec3<MathType> &inVertex,
                       const linalg::Vec2<MathType> &inTexCoord,
                       const MathType &inWeight,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        // std::cout << "calling vertex shader " << std::endl;
        gl_Position = t_matrix_ * linalg::Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        outVarying.depth = inVertex(2);
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

// -----------------------------------------------------------------------------
// ImageRendererBase
//   Another example derived class that might output color
// -----------------------------------------------------------------------------

template <typename MathType, typename ImageType, class Mesh, template <class> class Texture>
class ImageRendererBase
    : public RendererBase<MathType, ImageRendererBase<MathType, ImageType, Mesh, Texture>>
{
public:
    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
    };

    struct Textures
    {
        const Texture<ImageType> &in_texture;
        Texture<ImageType> &out_texture;
    };

    ImageRendererBase() = default;
    ~ImageRendererBase() = default;

    void Render(const Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                const Texture<ImageType> &in_texture,
                Texture<ImageType> &out_texture)
    {
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

        Textures textures{in_texture, out_texture};

        RendererBase<MathType, ImageRendererBase>::Render(mesh, viewport, textures);
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const MathType invW0, const MathType invW1, const MathType invW2,
                                  const MathType invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord * invW0 +
             w1 * varying_px1.texcoord * invW1 +
             w2 * varying_px2.texcoord * invW2) *
            (MathType(1) / invW_px);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const linalg::Vec3<MathType> &inVertex,
                       const linalg::Vec2<MathType> &inTexCoord,
                       const MathType &inWeight,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        gl_Position = t_matrix_ * linalg::Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        outVarying.texcoord = inTexCoord;
    }

    void fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        // MathType pix = textures.in_texture.sample_(in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        ImageType pix = sample<ImageType, Texture<ImageType>>(textures.in_texture, in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        // linalg::Vec2<MathType>coord;
        // coord(0) = in_varying.texcoord(0) * textures.in_texture.width(in_lvl_) - T(0.5);
        // coord(1) = in_varying.texcoord(1) * textures.in_texture.height(in_lvl_) - T(0.5);
        // MathType pix = bilinear<T, Texture<MathType>>(textures.in_texture, coord(1), coord(0), in_lvl_);

        if (pix == textures.in_texture.nodata())
            return;
        textures.out_texture.set_texel_(pix, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
    }

    linalg::Mat4<MathType> t_matrix_;
    unsigned int in_lvl_;
    unsigned int out_lvl_;
    // const Texture<MathType> *in_texture_;
    // Texture<MathType> *out_texture_;
};

template <typename MathType, typename ImageType, typename ErrorType, class Mesh, template <class> class Texture>
class ResidualRendererBase
    : public RendererBase<MathType, ResidualRendererBase<MathType, ImageType, ErrorType, Mesh, Texture>>
{
public:
    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
    };

    struct Textures
    {
        const Texture<ImageType> &kf_texture;
        const Texture<ImageType> &f_texture;
        Texture<ErrorType> &r_texture;
    };

    ResidualRendererBase() = default;
    ~ResidualRendererBase() = default;

    void Render(const Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                const Texture<ImageType> &kf_texture,
                const Texture<ImageType> &f_texture,
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

        Textures textures{kf_texture, f_texture, r_texture};

        RendererBase<MathType, ResidualRendererBase>::Render(mesh, viewport, textures);
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const MathType invW0, const MathType invW1, const MathType invW2,
                                  const MathType invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord * invW0 +
             w1 * varying_px1.texcoord * invW1 +
             w2 * varying_px2.texcoord * invW2) *
            (MathType(1) / invW_px);
        return var_over_w_px;
    }
    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const linalg::Vec3<MathType> &inVertex,
                       const linalg::Vec2<MathType> &inTexCoord,
                       const MathType &inWeight,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        gl_Position = t_matrix_ * linalg::Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        outVarying.texcoord = inTexCoord;
    }

    void fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        int width = textures.kf_texture.width(out_lvl_);
        int height = textures.kf_texture.height(out_lvl_);

        linalg::Vec2<MathType> screen_texcoord(gl_FragCoord(0) / MathType(width), gl_FragCoord(1) / MathType(height));

        ImageType kf = sample<ImageType, Texture<ImageType>>(textures.kf_texture, in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        ImageType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // float f = f_texture_->sample_(screen_texcoord(1), screen_texcoord(0), in_lvl_);

        if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata())
            return;

        ErrorType e = ErrorType(f) - ErrorType(kf);
        textures.r_texture.set_texel_(e, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
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
    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
    };

    struct Textures
    {
        const Texture<ImageType> &kf_texture;
        const Texture<ImageType> &f_texture;
        Texture<ErrorType> &r_texture;
    };

    L2RendererBase() = default;
    ~L2RendererBase() = default;

    void Render(const Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                const Texture<ImageType> &kf_texture,
                const Texture<ImageType> &f_texture,
                Texture<ErrorType> &r_texture)
    {
        r_texture.fill(out_lvl, r_texture.nodata());

        t_matrix_ = cam.GetProjectiveMatrix(RenderConstants::NEAR_PLANE, RenderConstants::FAR_PLANE) * this->opencv2opengl_ * pose.matrix();
        in_lvl_ = in_lvl;
        out_lvl_ = out_lvl;
        // kf_texture_ = &kf_texture;
        // f_texture_ = &f_texture;
        // r_texture_ = &r_texture;

        const int W = static_cast<int>(r_texture.width(out_lvl));
        const int H = static_cast<int>(r_texture.height(out_lvl));
        BoundingBox<int> viewport(0, W, 0, H);

        Textures textures{kf_texture, f_texture, r_texture};

        RendererBase<MathType, L2RendererBase>::Render(mesh, viewport, textures);
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const MathType invW0, const MathType invW1, const MathType invW2,
                                  const MathType invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord * invW0 +
             w1 * varying_px1.texcoord * invW1 +
             w2 * varying_px2.texcoord * invW2) *
            (MathType(1) / invW_px);
        return var_over_w_px;
    }
    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const linalg::Vec3<MathType> &inVertex,
                       const linalg::Vec2<MathType> &inTexCoord,
                       const MathType &inWeight,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        gl_Position = t_matrix_ * linalg::Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        outVarying.texcoord = inTexCoord;
    }

    void fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        unsigned int width = textures.kf_texture.width(out_lvl_);
        unsigned int height = textures.kf_texture.height(out_lvl_);

        linalg::Vec2<MathType> screen_texcoord(gl_FragCoord(0) / MathType(width), gl_FragCoord(1) / MathType(height));

        ImageType kf = sample<ImageType, Texture<ImageType>>(textures.kf_texture, in_varying.texcoord(1), in_varying.texcoord(0), in_lvl_);
        ImageType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // MathType f = sample<T, Texture<MathType>>(textures.f_texture, screen_texcoord(1), screen_texcoord(0), in_lvl_);

        if (kf == textures.kf_texture.nodata() || f == textures.f_texture.nodata())
            return;

        ErrorType e = ErrorType(f) - ErrorType(kf);
        textures.r_texture.set_texel_(e * e, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
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
    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
    };

    struct Textures
    {
        const Texture<ImageType> &in_texture;
        Texture<linalg::Vec3<DType>> &out_texture;
    };

    DIDxyRendererBase() = default;
    ~DIDxyRendererBase() = default;

    void Render(const Mesh &mesh,
                int in_lvl,
                int out_lvl,
                const Texture<ImageType> &in_texture,
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

        Textures textures{in_texture, out_texture};

        RendererBase<MathType, DIDxyRendererBase>::Render(mesh, viewport, textures);
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const MathType invW0, const MathType invW1, const MathType invW2,
                                  const MathType invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord * invW0 +
             w1 * varying_px1.texcoord * invW1 +
             w2 * varying_px2.texcoord * invW2) *
            (MathType(1) / invW_px);
        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const linalg::Vec3<MathType> &inVertex,
                       const linalg::Vec2<MathType> &inTexCoord,
                       const MathType &inWeight,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        // gl_Position = (view_matrix * pose_matrix) * linalg::Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = linalg::Vec4<MathType>(2.0 * inTexCoord(0) - 1.0, 2.0 * inTexCoord(1) - 1.0, 0.0, 1.0);
        outVarying.texcoord = inTexCoord;
    }

    void fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        // outFragment = inVarying;

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
    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
        linalg::Vec3<MathType> f_ver;
    };

    struct Textures
    {
        const Texture<ImageType> &kf_texture;
        const Texture<ImageType> &f_texture;
        const Texture<linalg::Vec3<DType>> &dfdxy_texture;
        Texture<linalg::Vec3<DType>> &jtra_texture;
        Texture<linalg::Vec3<DType>> &jrot_texture;
        Texture<ErrorType> &r_texture;
    };

    JPoseRendererBase() = default;
    ~JPoseRendererBase() = default;

    void Render(const Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                const Texture<ImageType> &kf_texture,
                const Texture<ImageType> &f_texture,
                const Texture<linalg::Vec3<DType>> &dfdxy_texture,
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

        Textures textures{kf_texture, f_texture, dfdxy_texture, jtra_texture, jrot_texture, r_texture};

        RendererBase<MathType, JPoseRendererBase>::Render(mesh, viewport, textures);
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const MathType invW0, const MathType invW1, const MathType invW2,
                                  const MathType invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord * invW0 +
             w1 * varying_px1.texcoord * invW1 +
             w2 * varying_px2.texcoord * invW2) *
            (MathType(1) / invW_px);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver * invW0 +
             w1 * varying_px1.f_ver * invW1 +
             w2 * varying_px2.f_ver * invW2) *
            (MathType(1) / invW_px);
        return var_over_w_px;
    }
    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const linalg::Vec3<MathType> &inVertex,
                       const linalg::Vec2<MathType> &inTexCoord,
                       const MathType &inWeight,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        linalg::Vec4<MathType> f_ver = pose_matrix_ * linalg::Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = view_matrix_ * f_ver;

        outVarying.f_ver = linalg::Vec3<MathType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.texcoord = inTexCoord;
    }

    void fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        unsigned int width = textures.kf_texture.width(out_lvl_);
        unsigned int height = textures.kf_texture.height(out_lvl_);

        linalg::Vec2<MathType> screen_texcoord(gl_FragCoord(0) / MathType(width), gl_FragCoord(1) / MathType(height));

        linalg::Vec3<MathType> f_ver = in_varying.f_ver;
        linalg::Vec2<MathType> texcoord = in_varying.texcoord;

        ImageType kf = sample<ImageType, Texture<ImageType>>(textures.kf_texture, texcoord(1), texcoord(0), in_lvl_);
        ImageType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        linalg::Vec3<DType> f_der = textures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // float f = sample<T, Texture<MathType>>(textures.f_texture, screen_texcoord(1), screen_texcoord(0), in_lvl_);
        // linalg::Vec3<MathType>f_der = sample<Vec3, Texture<Vec3>>(textures.dfdxy_texture, screen_texcoord(1), screen_texcoord(0), in_lvl_);

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
    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
        linalg::Vec3<MathType> f_ver;
        linalg::Vec3<MathType> kf_ray;
        MathType depth;
        linalg::Vec3<MathType> barycentric;
        unsigned int vertexId;
        linalg::Vec3<int> pids;
    };

    struct Textures
    {
        const Texture<ImageType> &kf_texture;
        const Texture<ImageType> &f_texture;
        const Texture<linalg::Vec3<DType>> &dfdxy_texture;
        Texture<linalg::Vec3<DType>> &jmap_texture;
        Texture<linalg::Vec3<IdType>> &pids_texture;
        Texture<ErrorType> &r_texture;
    };

    JMapRendererBase() = default;
    ~JMapRendererBase() = default;

    void Render(const Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                const Texture<ImageType> &kf_texture,
                const Texture<ImageType> &f_texture,
                const Texture<linalg::Vec3<DType>> &dfdxy_texture,
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

        Textures textures{kf_texture, f_texture, dfdxy_texture, jmap_texture, pids_texture, r_texture};

        RendererBase<MathType, JMapRendererBase>::Render(mesh, viewport, textures);
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const MathType invW0, const MathType invW1, const MathType invW2,
                                  const MathType invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord * invW0 +
             w1 * varying_px1.texcoord * invW1 +
             w2 * varying_px2.texcoord * invW2) *
            (MathType(1) / invW_px);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver * invW0 +
             w1 * varying_px1.f_ver * invW1 +
             w2 * varying_px2.f_ver * invW2) *
            (MathType(1) / invW_px);
        var_over_w_px.kf_ray =
            (w0 * varying_px0.kf_ray * invW0 +
             w1 * varying_px1.kf_ray * invW1 +
             w2 * varying_px2.kf_ray * invW2) *
            (MathType(1) / invW_px);
        // var_over_w_px.barycentric = linalg::Vec3<MathType>(w0 * invW0 * varying_px0.depth,
        //                                  w1 * invW1 * varying_px1.depth,
        //                                  w2 * invW2 * varying_px2.depth) *
        //                             (1.0f / invW_px);
        var_over_w_px.barycentric = linalg::Vec3<MathType>(w0 * invW0,
                                                           w1 * invW1,
                                                           w2 * invW2) *
                                    (MathType(1) / invW_px);
        var_over_w_px.pids = linalg::Vec3<int>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const linalg::Vec3<MathType> &inVertex,
                       const linalg::Vec2<MathType> &inTexCoord,
                       const float &inWeight,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        linalg::Vec4<MathType> f_ver = pose_matrix_ * linalg::Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = view_matrix_ * f_ver;

        linalg::Vec3<MathType> kf_ray(inVertex(0) / inVertex(2), inVertex(1) / inVertex(2), MathType(1));
        linalg::Vec4<MathType> d_f_ver_d_kf_depth_ = pose_matrix_ * linalg::Vec4<MathType>(kf_ray(0), kf_ray(1), kf_ray(2), MathType(0));
        linalg::Vec3<MathType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = linalg::Vec3<MathType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = inVertex(2);
        outVarying.vertexId = vertexid;
        outVarying.texcoord = inTexCoord;
    }

    void fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        unsigned int width = textures.jmap_texture.width(out_lvl_);
        unsigned int height = textures.jmap_texture.height(out_lvl_);

        linalg::Vec2<MathType> screen_texcoord(gl_FragCoord(0) / MathType(width), gl_FragCoord(1) / MathType(height));

        linalg::Vec3<MathType> f_ver = in_varying.f_ver;
        linalg::Vec3<MathType> kf_ray = in_varying.kf_ray;
        linalg::Vec2<MathType> texcoord = in_varying.texcoord;
        linalg::Vec3<MathType> barycentric = in_varying.barycentric;
        linalg::Vec3<int> vertexid = in_varying.pids;

        ImageType kf = sample<ImageType, Texture<ImageType>>(textures.kf_texture, texcoord(1), texcoord(0), in_lvl_);
        ImageType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        linalg::Vec3<DType> f_der = textures.dfdxy_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        // float f = f_texture_->sample_(screen_texcoord(1), screen_texcoord(0), in_lvl_);
        // linalg::Vec3<MathType>f_der = dfdxy_texture_->sample_(screen_texcoord(1), screen_texcoord(0), in_lvl_);

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

        linalg::Vec3<MathType> d_depth_d_vert_depth = barycentric;

        linalg::Vec3<MathType> jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;
        linalg::Vec3<IdType> ids = linalg::Vec3<IdType>(vertexid(0), vertexid(1), vertexid(2));

        textures.jmap_texture.set_texel_(jac, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.pids_texture.set_texel_(ids, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.r_texture.set_texel_(r, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
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
    struct Varyings
    {
        linalg::Vec2<MathType> texcoord;
        linalg::Vec3<MathType> f_ver;
        linalg::Vec3<MathType> kf_ray;
        MathType depth;
        linalg::Vec3<MathType> barycentric;
        unsigned int vertexId;
        linalg::Vec3<int> pids;
    };

    struct Textures
    {
        const Texture<ImageType> &f_texture;
        Texture<ImageType> &image_texture;
        Texture<DepthType> &depth_texture;
        Texture<linalg::Vec3<DType>> &jtra_texture;
        Texture<linalg::Vec3<DType>> &jrot_texture;
        Texture<linalg::Vec3<DType>> &jmap_texture;
        Texture<linalg::Vec3<IdType>> &pids_texture;
    };

    DiffRendererBase() = default;
    ~DiffRendererBase() = default;

    void Render(const Mesh &mesh,
                const linalg::SE3<MathType> &pose,
                const Camera<MathType> &cam,
                int in_lvl,
                int out_lvl,
                const Texture<ImageType> &f_texture,
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

        Textures textures{f_texture, image_texture, depth_texture, jtra_texture, jrot_texture, jmap_texture, pids_texture};

        RendererBase<MathType, DiffRendererBase>::Render(mesh, viewport, textures);
    }

    Varyings interpolate_varyings(const MathType w0, const MathType w1, const MathType w2,
                                  const MathType invW0, const MathType invW1, const MathType invW2,
                                  const MathType invW_px,
                                  const Varyings &varying_px0,
                                  const Varyings &varying_px1,
                                  const Varyings &varying_px2)
    {
        Varyings var_over_w_px;
        var_over_w_px.texcoord =
            (w0 * varying_px0.texcoord * invW0 +
             w1 * varying_px1.texcoord * invW1 +
             w2 * varying_px2.texcoord * invW2) *
            (MathType(1) / invW_px);
        var_over_w_px.f_ver =
            (w0 * varying_px0.f_ver * invW0 +
             w1 * varying_px1.f_ver * invW1 +
             w2 * varying_px2.f_ver * invW2) *
            (MathType(1) / invW_px);
        var_over_w_px.kf_ray =
            (w0 * varying_px0.kf_ray * invW0 +
             w1 * varying_px1.kf_ray * invW1 +
             w2 * varying_px2.kf_ray * invW2) *
            (MathType(1) / invW_px);
        var_over_w_px.barycentric = linalg::Vec3<MathType>(w0 * invW0,
                                                           w1 * invW1,
                                                           w2 * invW2) *
                                    (MathType(1) / invW_px);

        var_over_w_px.pids = linalg::Vec3<int>(varying_px0.vertexId, varying_px1.vertexId, varying_px2.vertexId);

        return var_over_w_px;
    }

    // -------------------------------------------------------------------------
    // Shaders
    // -------------------------------------------------------------------------
    void vertex_shader(const linalg::Vec3<MathType> &inVertex,
                       const linalg::Vec2<MathType> &inTexCoord,
                       const MathType &inWeight,
                       const unsigned int &vertexid,
                       linalg::Vec4<MathType> &gl_Position,
                       Varyings &outVarying)
    {
        linalg::Vec4<MathType> f_ver = pose_matrix_ * linalg::Vec4<MathType>(inVertex(0), inVertex(1), inVertex(2), 1.0f);
        gl_Position = view_matrix_ * f_ver;

        linalg::Vec3<MathType> kf_ray(inVertex(0) / inVertex(2), inVertex(1) / inVertex(2), MathType(1));
        linalg::Vec4<MathType> d_f_ver_d_kf_depth_ = pose_matrix_ * linalg::Vec4<MathType>(kf_ray(0), kf_ray(1), kf_ray(2), 0.0);
        linalg::Vec3<MathType> d_f_ver_d_kf_depth(d_f_ver_d_kf_depth_(0), d_f_ver_d_kf_depth_(1), d_f_ver_d_kf_depth_(2));

        outVarying.f_ver = linalg::Vec3<MathType>(f_ver(0), f_ver(1), f_ver(2));
        outVarying.kf_ray = d_f_ver_d_kf_depth;
        outVarying.depth = inVertex(2);
        outVarying.vertexId = vertexid;
        outVarying.texcoord = inTexCoord;
    }

    void fragment_shader(const linalg::Vec4<MathType> &gl_FragCoord,
                         const Varyings &in_varying,
                         Textures &textures)
    {
        unsigned int width = textures.jmap_texture.width(out_lvl_);
        unsigned int height = textures.jmap_texture.height(out_lvl_);

        // linalg::Vec2<MathType>screen_texcoord(gl_FragCoord(0) / T(width), gl_FragCoord(1) / T(height));

        linalg::Vec3<MathType> f_ver = in_varying.f_ver;
        linalg::Vec3<MathType> kf_ray = in_varying.kf_ray;
        linalg::Vec2<MathType> texcoord = in_varying.texcoord;
        linalg::Vec3<MathType> barycentric = in_varying.barycentric;
        linalg::Vec3<int> vertexid = in_varying.pids;

        // MathType f = textures.f_texture.texel_(gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        ImageType f = sample<ImageType, Texture<ImageType>>(textures.f_texture, in_varying.texcoord(1), in_varying.texcoord(0), out_lvl_);
        if (f == textures.f_texture.nodata())
            return;

        linalg::Vec3<DType> f_der = compute_didxy(textures.f_texture, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);

        if (f_der(0) == textures.f_texture.nodata() && f_der(1) == textures.f_texture.nodata())
            return;

        linalg::Vec3<MathType> d_f_i_d_f_ver;

        d_f_i_d_f_ver(0) = f_der(0) * fx_ * width / f_ver(2);
        d_f_i_d_f_ver(1) = f_der(1) * fy_ * height / f_ver(2);
        d_f_i_d_f_ver(2) = -(d_f_i_d_f_ver(0) * f_ver(0) + d_f_i_d_f_ver(1) * f_ver(1)) / f_ver(2);

        // linalg::Vec3<MathType>d_f_i_d_tra = linalg::Vec3<MathType>(v0, v1, v2);
        linalg::Vec3<MathType> d_f_i_d_rot = linalg::Vec3<MathType>(-f_ver(2) * d_f_i_d_f_ver(1) + f_ver(1) * d_f_i_d_f_ver(2), f_ver(2) * d_f_i_d_f_ver(0) - f_ver(0) * d_f_i_d_f_ver(2), -f_ver(1) * d_f_i_d_f_ver(0) + f_ver(0) * d_f_i_d_f_ver(1));

        linalg::Vec3<MathType> d_f_ver_d_kf_depth = kf_ray; // kfTofPose.rotationMatrix() * kf_ray;
        MathType d_f_i_d_kf_depth = (d_f_i_d_f_ver.transpose() * d_f_ver_d_kf_depth)(0, 0);

        linalg::Vec3<MathType> d_depth_d_vert_depth = barycentric;

        linalg::Vec3<MathType> jac = d_f_i_d_kf_depth * d_depth_d_vert_depth;
        linalg::Vec3<IdType> ids = linalg::Vec3<IdType>(vertexid(0), vertexid(1), vertexid(2));

        textures.image_texture.set_texel_(f, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.depth_texture.set_texel_(f_ver(2), gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.jtra_texture.set_texel_(d_f_i_d_f_ver, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.jrot_texture.set_texel_(d_f_i_d_rot, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.jmap_texture.set_texel_(jac, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
        textures.pids_texture.set_texel_(ids, gl_FragCoord(1), gl_FragCoord(0), out_lvl_);
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
