#pragma once

// #include "core/types.h"
#include <Eigen/Core>
#include "core/render_constants.h"
#include "core/camera.h"
#include "core/delaunaytriangulation.h"
#include "backends/cpu/texturecpu.h"

static std::vector<Vec2<float>> UniformTexCoords(int width, int height, float min_x, float min_y, float max_x, float max_y)
{
    std::vector<Vec2<float>> texcoords;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            Vec2<float> pix;
            pix(0) = (float(x) / (width - 1)) * (max_x - min_x) + min_x;
            pix(1) = (float(y) / (height - 1)) * (max_y - min_y) + min_y;

            texcoords.push_back(pix);
        }
    }

    return texcoords;
}

static float RandomDepth(float min_depth, float max_depth)
{
    float depth = (max_depth - min_depth) * float(rand() % 1000) / 1000.0 + min_depth;
    return depth;
}

static float VerticallySmoothDepth(Vec2<float> pix, float min_depth, float max_depth)
{
    // max depth when y = 0
    float depth = max_depth + (min_depth - max_depth) * pix(1);
    if (depth < min_depth)
        depth = min_depth;
    if (depth > max_depth)
        depth = max_depth;
    return depth;
}

static void BuildTriangles(const std::vector<Vec2<float>> &tex_coords, std::vector<int> &tris_f)
{
    DelaunayTriangulation triangulator_;
    triangulator_.LoadPoints(tex_coords);
    triangulator_.Triangulate();
    std::vector<Vec3<int>> tris = triangulator_.GetTriangles();
    tris_f.clear();
    tris_f.reserve(tris.size() * 3);
    for (size_t i = 0; i < tris.size(); i++)
    {
        tris_f.push_back(tris[i](0));
        tris_f.push_back(tris[i](1));
        tris_f.push_back(tris[i](2));
    }
}

// Screen quad for image-space rendering
static void CreateScreenQuad(std::vector<float> &vertex,
                             std::vector<int> &indices)
{
    vertex = {-1.f, 1.f, 1.f, 0.f, 1.f,
              -1.f, -1.f, 1.f, 0.f, 0.f,
              1.f, -1.f, 1.f, 1.f, 0.f,
              -1.f, 1.f, 1.f, 0.f, 1.f,
              1.f, -1.f, 1.f, 1.f, 0.f,
              1.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<Vec2<float>> uv = {{0.f, 1.f}, {0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f}, {1.f, 0.f}, {1.f, 1.f}};
    // indices = {0, 1, 2, 0, 2, 3};
    BuildTriangles(uv, indices);
}

template <class Texture>
static void CreateMesh(const Texture &depth,
                       PinholeCamera<float> &cam, int grid_size,
                       std::vector<float> &vertex,
                       std::vector<int> &indices,
                       bool add_pos = true,
                       bool add_tex = true,
                       bool add_normal = true)
{
    std::vector<Vec2<float>> grid_uv = UniformTexCoords(grid_size, grid_size, 0.0, 0.0, 1.0, 1.0);

    int stride = 0;
    if (add_pos)
        stride += 3;
    if (add_tex)
        stride += 2;
    if (add_normal)
        stride += 3;

    vertex.clear();
    vertex.reserve(grid_uv.size() * stride);

    const int w = depth.width(0);
    const int h = depth.height(0);

    auto depth_mm = depth.MapRead(0);

    // UV step for the grid (neighbors)
    const float du = 5.0f / float(w - 1);
    const float dv = 5.0f / float(h - 1);

    auto clamp01 = [](float x)
    { return std::max(0.0f, std::min(1.0f, x)); };

    // Sample a 3D point from (u,v) using nearest-neighbor depth. Returns false if invalid.
    auto sample_pos = [&](float u, float v, Vec3<float> &out) -> bool
    {
        // u = clamp01(u);
        // v = clamp01(v);

        if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
            return false;

        const float ix = u * float(w - 1);
        const float iy = v * float(h - 1);
        const int x = static_cast<int>(ix + 0.5f); // nearest; switch to bilinear if you like
        const int y = static_cast<int>(iy + 0.5f);

        float z = depth_mm[y * w + x];
        if (z == depth.nodata())
        {
            return false;
            // z = (RenderConstants::FAR_PLANE - RenderConstants::NEAR_PLANE) / 2.0;
            // z = 1.0f;
        }

        if (z < RenderConstants::NEAR_PLANE)
            z = RenderConstants::NEAR_PLANE;
        if (z > RenderConstants::FAR_PLANE)
            z = RenderConstants::FAR_PLANE;

        const Vec2<float> uv{u, v};
        const Vec3<float> ray = cam.PixToRay(uv);
        out = ray * z; // camera/world space position along the ray
        return true;
    };

    std::vector<Vec2<float>> ok_uv;

    for (const Vec2<float> &uv : grid_uv)
    {
        const float u = uv(0), v = uv(1);

        // Center point
        Vec3<float> P;
        if (!sample_pos(u, v, P))
            continue; // skip invalid vertex entirely (keep this if your pipeline expects sparse vertices)

        // Neighbors for finite differences
        Vec3<float> PR, PL, PU, PD;
        const bool hasR = sample_pos(u + du, v, PR);
        const bool hasL = sample_pos(u - du, v, PL);
        const bool hasU = sample_pos(u, v + dv, PU);
        const bool hasD = sample_pos(u, v - dv, PD);

        // Tangents in world/camera space
        Vec3<float> dUvec{0, 0, 0};
        Vec3<float> dVvec{0, 0, 0};

        if (hasR && hasL)
            dUvec = PR - PL; // central diff
        else if (hasR)
            dUvec = PR - P; // forward diff
        else if (hasL)
            dUvec = P - PL; // backward diff
        else
            continue;

        if (hasU && hasD)
            dVvec = PU - PD;
        else if (hasU)
            dVvec = PU - P;
        else if (hasD)
            dVvec = P - PD;
        else
            continue;

        // Normal from cross product (right-handed): n = normalize(dU x dV)
        Vec3<float> N = -dUvec.cross(dVvec);
        const float len2 = N.norm();
        if (len2 > 1e-12f)
        {
            N = N / len2;
        }
        else
        {
            // Degenerate neighborhood: fall back to a view-facing normal
            const Vec3<float> ray = -cam.PixToRay(uv);
            N = ray.normalized();
        }

        if (add_pos)
        {
            vertex.push_back(P(0));
            vertex.push_back(P(1));
            vertex.push_back(P(2));
        }
        if (add_tex)
        {
            vertex.push_back(u);
            vertex.push_back(v);
        }
        if (add_normal)
        {
            vertex.push_back(N(0));
            vertex.push_back(N(1));
            vertex.push_back(N(2));
        }

        ok_uv.push_back(uv);
    }

    BuildTriangles(ok_uv, indices);
}

static void CreateFlatMesh(float min_depth, float max_depth,
                           PinholeCamera<float> &cam, int grid_size,
                           std::vector<float> &vertex,
                           std::vector<int> &indices,
                           bool add_pos = true,
                           bool add_tex = true,
                           bool add_normal = true)
{
    std::vector<Vec2<float>> grid_uv = UniformTexCoords(grid_size, grid_size, -0.0, -0.0, 1.0, 1.0);

    int gridsize = grid_uv.size();

    int stride = 0;
    if (add_pos)
        stride += 3;
    if (add_tex)
        stride += 2;
    if (add_normal)
        stride += 3;

    vertex.clear();
    vertex.reserve(gridsize * stride);

    for (const Vec2<float> &uv : grid_uv)
    {
        const float depth = VerticallySmoothDepth(uv, min_depth, max_depth);

        if (depth <= 0.0f)
            continue;

        const Vec3<float> ray = cam.PixToRay(uv);
        const Vec3<float> ver = ray * depth;
        const Vec3<float> nor(0.0, 0.0, 1.0);

        if (add_pos)
        {
            vertex.push_back(ver(0));
            vertex.push_back(ver(1));
            vertex.push_back(ver(2));
        }
        if (add_tex)
        {
            vertex.push_back(uv(0));
            vertex.push_back(uv(1));
        }
        if (add_normal)
        {
            vertex.push_back(nor(0));
            vertex.push_back(nor(1));
            vertex.push_back(nor(2));
        }
    }

    BuildTriangles(grid_uv, indices);
}