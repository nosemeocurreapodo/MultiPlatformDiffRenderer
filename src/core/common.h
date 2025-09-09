#pragma once

#include "core/types.h"
#include "core/delaunaytriangulation.h"
#include "backends/cpu/texturecpu.h"
#include "backends/cpu/meshcpu.h"

inline std::vector<Vec2> UniformTexCoords(int width, int height)
{
    std::vector<Vec2> texcoords;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            Vec2 pix;
            pix(0) = Scalar(x) / (width - 1);
            pix(1) = Scalar(y) / (height - 1);

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

inline float VerticallySmoothDepth(Vec2 pix, float min_depth, float max_depth)
{
    // max depth when y = 0
    float depth = max_depth + (min_depth - max_depth) * pix(1);
    return depth;
}

inline std::vector<unsigned int> BuildTriangles(const std::vector<float> &tex_coords)
{
    DelaunayTriangulation triangulator_;
    std::vector<Vec2> tex_coords_2d;
    for (size_t i = 0; i < tex_coords.size(); i += 2)
    {
        tex_coords_2d.push_back(Vec2(tex_coords[i], tex_coords[i + 1]));
    }
    triangulator_.LoadPoints(tex_coords_2d);
    triangulator_.Triangulate();
    std::vector<Vec3i> tris = triangulator_.GetTriangles();
    std::vector<unsigned int> tris_f;
    for (size_t i = 0; i < tris.size(); i++)
    {
        tris_f.push_back(tris[i](0));
        tris_f.push_back(tris[i](1));
        tris_f.push_back(tris[i](2));
    }
    return tris_f;
}

// Screen quad for image-space rendering
inline void CreateScreenQuad(std::vector<float> &pos,
                             std::vector<float> &uv,
                             std::vector<float> &weights,
                             std::vector<unsigned int> &indices)
{
    pos = {-1.f, 1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f, 1.f,
           -1.f, 1.f, 1.f, 1.f, -1.f, 1.f, 1.f, 1.f, 1.f};
    uv = {0.f, 1.f, 0.f, 0.f, 1.f, 0.f,
          0.f, 1.f, 1.f, 0.f, 1.f, 1.f};
    weights.assign(6, 1.0f);
    // indices = {0, 1, 2, 0, 2, 3};
    indices = BuildTriangles(uv);
}

inline void CreateMesh(const TextureCPU<float> &depth,
                       Camera &cam, int grid_size,
                       std::vector<float> &vertices,
                       std::vector<float> &texcoords,
                       std::vector<float> &weights,
                       std::vector<unsigned int> &indices)
{
    std::vector<Vec2> grid_uv = UniformTexCoords(grid_size, grid_size);

    vertices.clear();
    texcoords.clear();
    weights.clear();

    vertices.reserve(grid_uv.size() * 3);
    texcoords.reserve(grid_uv.size() * 2);
    weights.reserve(grid_uv.size());

    int w = depth.width(0);
    int h = depth.height(0);
    auto depth_mm = depth.MapRead(0);

    for (const Vec2 &uv : grid_uv)
    {
        const float ix = uv(0) * (w - 1);
        const float iy = uv(1) * (h - 1);
        const int x = static_cast<int>(ix);
        const int y = static_cast<int>(iy);
        const float depth = depth_mm[y * w + x];

        if (depth <= 0.0f)
            continue;

        const Vec3 ray = cam.PixToRay(uv);
        const Vec3 vertex = ray * depth;

        vertices.push_back(vertex(0));
        vertices.push_back(vertex(1));
        vertices.push_back(vertex(2));
        texcoords.push_back(uv(0));
        texcoords.push_back(uv(1));
        weights.push_back(1.0f);
    }

    indices = BuildTriangles(texcoords);
}

inline void CreateFlatMesh(float min_depth, float max_depth,
                           Camera &cam, int grid_size,
                           std::vector<float> &vertices,
                           std::vector<float> &texcoords,
                           std::vector<float> &weights,
                           std::vector<unsigned int> &indices)
{
    std::vector<Vec2> grid_uv = UniformTexCoords(grid_size, grid_size);

    vertices.clear();
    texcoords.clear();
    weights.clear();

    vertices.reserve(grid_uv.size() * 3);
    texcoords.reserve(grid_uv.size() * 2);
    weights.reserve(grid_uv.size());

    for (const Vec2 &uv : grid_uv)
    {
        const float depth = VerticallySmoothDepth(uv, min_depth, max_depth);

        if (depth <= 0.0f)
            continue;

        const Vec3 ray = cam.PixToRay(uv);
        const Vec3 vertex = ray * depth;

        vertices.push_back(vertex(0));
        vertices.push_back(vertex(1));
        vertices.push_back(vertex(2));
        texcoords.push_back(uv(0));
        texcoords.push_back(uv(1));
        weights.push_back(1.0f);
    }

    indices = BuildTriangles(texcoords);
}

inline void CreateSphereMesh(float depth,
                             Camera &cam, int grid_size,
                             std::vector<float> &vertices,
                             std::vector<float> &texcoords,
                             std::vector<float> &weights,
                             std::vector<unsigned int> &indices)
{
    std::vector<Vec2> grid_uv = UniformTexCoords(grid_size, grid_size);

    vertices.clear();
    texcoords.clear();
    weights.clear();

    vertices.reserve(grid_uv.size() * 3);
    texcoords.reserve(grid_uv.size() * 2);
    weights.reserve(grid_uv.size());

    for (const Vec2 &uv : grid_uv)
    {
        if (depth <= 0.0f)
            continue;

        const Vec3 ray = cam.PixToRay(uv).normalized();
        const Vec3 vertex = ray * depth;

        vertices.push_back(vertex(0));
        vertices.push_back(vertex(1));
        vertices.push_back(vertex(2));
        texcoords.push_back(uv(0));
        texcoords.push_back(uv(1));
        weights.push_back(1.0f);
    }

    indices = BuildTriangles(texcoords);
}
