#pragma once

// #include "core/types.h"
#include "core/camera.h"
#include "core/delaunaytriangulation.h"
#include "backends/cpu/texturecpu.h"
#include "backends/cpu/meshcpu.h"

inline std::vector<linalg::Vec2<float>> UniformTexCoords(int width, int height)
{
    std::vector<linalg::Vec2<float>> texcoords;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            linalg::Vec2<float> pix;
            pix(0) = float(x) / (width - 1);
            pix(1) = float(y) / (height - 1);

            texcoords.push_back(pix);
        }
    }

    return texcoords;
}

inline float RandomDepth(float min_depth, float max_depth)
{
    float depth = (max_depth - min_depth) * float(rand() % 1000) / 1000.0 + min_depth;
    return depth;
}

inline float VerticallySmoothDepth(linalg::Vec2<float> pix, float min_depth, float max_depth)
{
    // max depth when y = 0
    float depth = max_depth + (min_depth - max_depth) * pix(1);
    return depth;
}

inline void BuildTriangles(const std::vector<float> &tex_coords, std::vector<unsigned int> &tris_f)
{
    DelaunayTriangulation triangulator_;
    std::vector<linalg::Vec2<float>> tex_coords_2d;
    for (size_t i = 0; i < tex_coords.size(); i += 2)
    {
        tex_coords_2d.push_back(linalg::Vec2<float>(tex_coords[i], tex_coords[i + 1]));
    }
    triangulator_.LoadPoints(tex_coords_2d);
    triangulator_.Triangulate();
    std::vector<linalg::Vec3<int>> tris = triangulator_.GetTriangles();
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
inline void CreateScreenQuad(std::vector<float> &pos,
                             std::vector<float> &uv,
                             std::vector<unsigned int> &indices)
{
    pos = {-1.f, 1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f, 1.f,
           -1.f, 1.f, 1.f, 1.f, -1.f, 1.f, 1.f, 1.f, 1.f};
    uv = {0.f, 1.f, 0.f, 0.f, 1.f, 0.f,
          0.f, 1.f, 1.f, 0.f, 1.f, 1.f};
    // indices = {0, 1, 2, 0, 2, 3};
    BuildTriangles(uv, indices);
}

inline void CreateMesh(const TextureCPU<float> &depth,
                       Camera<float> &cam, int grid_size,
                       std::vector<float> &vertices,
                       std::vector<float> &normals, // <-- NEW
                       std::vector<float> &texcoords,
                       std::vector<float> &weights,
                       std::vector<unsigned int> &indices)
{
    std::vector<linalg::Vec2<float>> grid_uv = UniformTexCoords(grid_size, grid_size);

    vertices.clear();
    texcoords.clear();
    weights.clear();
    normals.clear(); // <-- NEW

    vertices.reserve(grid_uv.size() * 3);
    texcoords.reserve(grid_uv.size() * 2);
    weights.reserve(grid_uv.size());
    normals.reserve(grid_uv.size() * 3); // <-- NEW

    const int w = depth.width(0);
    const int h = depth.height(0);
    auto depth_mm = depth.MapRead(0);

    // UV step for the grid (neighbors)
    const float du = (grid_size > 1) ? 1.0f / float(grid_size - 1) : 1.0f;
    const float dv = du;

    auto clamp01 = [](float x)
    { return std::max(0.0f, std::min(1.0f, x)); };

    // Sample a 3D point from (u,v) using nearest-neighbor depth. Returns false if invalid.
    auto sample_pos = [&](float u, float v, linalg::Vec3<float> &out) -> bool
    {
        u = clamp01(u);
        v = clamp01(v);

        const float ix = u * float(w - 1);
        const float iy = v * float(h - 1);
        const int x = static_cast<int>(ix + 0.5f); // nearest; switch to bilinear if you like
        const int y = static_cast<int>(iy + 0.5f);

        const float z = depth_mm[y * w + x];
        if (z <= 0.0f)
            return false;

        const linalg::Vec2<float> uv{u, v};
        const linalg::Vec3<float> ray = cam.PixToRay(uv);
        out = ray * z; // camera/world space position along the ray
        return true;
    };

    for (const linalg::Vec2<float> &uv : grid_uv)
    {
        const float u = uv(0), v = uv(1);

        // Center point
        linalg::Vec3<float> P;
        if (!sample_pos(u, v, P))
            continue; // skip invalid vertex entirely (keep this if your pipeline expects sparse vertices)

        // Neighbors for finite differences
        linalg::Vec3<float> PR, PL, PU, PD;
        const bool hasR = sample_pos(u + du, v, PR);
        const bool hasL = sample_pos(u - du, v, PL);
        const bool hasU = sample_pos(u, v + dv, PU);
        const bool hasD = sample_pos(u, v - dv, PD);

        // Tangents in world/camera space
        linalg::Vec3<float> dUvec{0, 0, 0};
        linalg::Vec3<float> dVvec{0, 0, 0};

        if (hasR && hasL)
            dUvec = PR - PL; // central diff
        else if (hasR)
            dUvec = PR - P; // forward diff
        else if (hasL)
            dUvec = P - PL; // backward diff

        if (hasU && hasD)
            dVvec = PU - PD;
        else if (hasU)
            dVvec = PU - P;
        else if (hasD)
            dVvec = P - PD;

        // Normal from cross product (right-handed): n = normalize(dU x dV)
        linalg::Vec3<float> N = dUvec.cross(dVvec);
        const float len2 = N(0) * N(0) + N(1) * N(1) + N(2) * N(2);
        if (len2 > 1e-12f)
        {
            const float invLen = 1.0f / std::sqrt(len2);
            N = N * invLen;
        }
        else
        {
            // Degenerate neighborhood: fall back to a view-facing normal
            const linalg::Vec3<float> ray = -cam.PixToRay(uv);
            N = ray.normalized();
        }

        // Output streams
        vertices.push_back(P(0));
        vertices.push_back(P(1));
        vertices.push_back(P(2));

        texcoords.push_back(u);
        texcoords.push_back(v);

        normals.push_back(N(0)); // <-- NEW
        normals.push_back(N(1));
        normals.push_back(N(2));

        weights.push_back(1.0f);
    }

    BuildTriangles(texcoords, indices);
}

inline void CreateFlatMesh(float min_depth, float max_depth,
                           Camera<float> &cam, int grid_size,
                           std::vector<float> &vertices,
                           std::vector<float> &texcoords,
                           std::vector<float> &weights,
                           std::vector<unsigned int> &indices)
{
    std::vector<linalg::Vec2<float>> grid_uv = UniformTexCoords(grid_size, grid_size);

    vertices.clear();
    texcoords.clear();
    weights.clear();

    vertices.reserve(grid_uv.size() * 3);
    texcoords.reserve(grid_uv.size() * 2);
    weights.reserve(grid_uv.size());

    for (const linalg::Vec2<float> &uv : grid_uv)
    {
        const float depth = VerticallySmoothDepth(uv, min_depth, max_depth);

        if (depth <= 0.0f)
            continue;

        const linalg::Vec3<float> ray = cam.PixToRay(uv);
        const linalg::Vec3<float> vertex = ray * depth;

        vertices.push_back(vertex(0));
        vertices.push_back(vertex(1));
        vertices.push_back(vertex(2));
        texcoords.push_back(uv(0));
        texcoords.push_back(uv(1));
        weights.push_back(1.0f);
    }

    BuildTriangles(texcoords, indices);
}

inline void CreateSphereMesh(float depth,
                             Camera<float> &cam, int grid_size,
                             std::vector<float> &vertices,
                             std::vector<float> &texcoords,
                             std::vector<float> &weights,
                             std::vector<unsigned int> &indices)
{
    std::vector<linalg::Vec2<float>> grid_uv = UniformTexCoords(grid_size, grid_size);

    vertices.clear();
    texcoords.clear();
    weights.clear();

    vertices.reserve(grid_uv.size() * 3);
    texcoords.reserve(grid_uv.size() * 2);
    weights.reserve(grid_uv.size());

    for (const linalg::Vec2<float> &uv : grid_uv)
    {
        if (depth <= 0.0f)
            continue;

        linalg::Vec3<float> ray = cam.PixToRay(uv);
        ray = ray / ray.norm();
        const linalg::Vec3<float> vertex = ray * depth;

        vertices.push_back(vertex(0));
        vertices.push_back(vertex(1));
        vertices.push_back(vertex(2));
        texcoords.push_back(uv(0));
        texcoords.push_back(uv(1));
        weights.push_back(1.0f);
    }

    BuildTriangles(texcoords, indices);
}
