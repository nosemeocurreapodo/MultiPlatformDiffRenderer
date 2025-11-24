#pragma once

// #include "core/types.h"
#include <Eigen/Core>
#include "core/camera.h"
#include "core/delaunaytriangulation.h"
#include "backends/cpu/texturecpu.h"

std::vector<Vec2<float>> UniformTexCoords(int width, int height)
{
    std::vector<Vec2<float>> texcoords;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            Vec2<float> pix;
            pix(0) = float(x) / (width - 1);
            pix(1) = float(y) / (height - 1);

            texcoords.push_back(pix);
        }
    }

    return texcoords;
}

float RandomDepth(float min_depth, float max_depth)
{
    float depth = (max_depth - min_depth) * float(rand() % 1000) / 1000.0 + min_depth;
    return depth;
}

float VerticallySmoothDepth(Vec2<float> pix, float min_depth, float max_depth)
{
    // max depth when y = 0
    float depth = max_depth + (min_depth - max_depth) * pix(1);
    return depth;
}

void BuildTriangles(const std::vector<Eigen::Vector2f> &tex_coords, std::vector<int> &tris_f)
{
    DelaunayTriangulation triangulator_;
    std::vector<Vec2<float>> tex_coords_2d;
    for (size_t i = 0; i < tex_coords.size(); i++)
    {
        tex_coords_2d.push_back(Vec2<float>(tex_coords[i].x(), tex_coords[i].y()));
    }
    triangulator_.LoadPoints(tex_coords_2d);
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
void CreateScreenQuad(std::vector<float> &vertex,
                      std::vector<int> &indices)
{
    vertex = {-1.f, 1.f, 1.f, 0.f, 1.f,
              -1.f, -1.f, 1.f, 0.f, 0.f,
              1.f, -1.f, 1.f, 1.f, 0.f,
              -1.f, 1.f, 1.f, 0.f, 1.f,
              1.f, -1.f, 1.f, 1.f, 0.f,
              1.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<Eigen::Vector2f> uv = {{0.f, 1.f}, {0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f}, {1.f, 0.f}, {1.f, 1.f}};
    // indices = {0, 1, 2, 0, 2, 3};
    BuildTriangles(uv, indices);
}

void CreateMesh(const cv::Mat &depth,
                PinholeCamera<float> &cam, int grid_size,
                std::vector<float> &vertex,
                std::vector<int> &indices,
                bool add_pos = true,
                bool add_tex = true,
                bool add_normal = true)
{
    std::vector<Vec2<float>> grid_uv = UniformTexCoords(grid_size, grid_size);

    int stride = 0;
    if (add_pos)
        stride += 3;
    if (add_tex)
        stride += 2;
    if (add_normal)
        stride += 3;

    std::vector<Eigen::Vector2f> texcoords;
    texcoords.reserve(grid_uv.size());

    vertex.clear();
    vertex.reserve(grid_uv.size() * stride);

    const int w = depth.cols;
    const int h = depth.rows;

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

        const float z = depth.at<float>(y, x);
        if (z <= 0.0f)
            return false;

        const Vec2<float> uv{u, v};
        const Vec3<float> ray = cam.PixToRay(uv);
        out = ray * z; // camera/world space position along the ray
        return true;
    };

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

        texcoords.push_back(Eigen::Vector2f(u, v));
    }

    BuildTriangles(texcoords, indices);
}

void CreateMesh(const TextureCPU<float> &depth,
                PinholeCamera<float> &cam, int grid_size,
                std::vector<Eigen::Vector3f> &vertices,
                std::vector<Eigen::Vector2f> &texcoords,
                std::vector<Eigen::Vector3f> &normals,
                std::vector<int> &indices)
{
    std::vector<Vec2<float>> grid_uv = UniformTexCoords(grid_size, grid_size);

    vertices.clear();
    texcoords.clear();
    normals.clear();

    vertices.reserve(grid_uv.size());
    texcoords.reserve(grid_uv.size());
    normals.reserve(grid_uv.size()); // <-- NEW

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

        const float z = depth_mm[y * w + x];
        if (z <= 0.0f || z == depth.nodata())
            return false;

        const Vec2<float> uv{u, v};
        const Vec3<float> ray = cam.PixToRay(uv);
        out = ray * z; // camera/world space position along the ray
        return true;
    };

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

        // Output streams
        vertices.push_back(Eigen::Vector3f(P(0), P(1), P(2)));
        texcoords.push_back(Eigen::Vector2f(u, v));
        normals.push_back(Eigen::Vector3f(N(0), N(1), N(2))); // <-- NEW
    }

    BuildTriangles(texcoords, indices);
}

void CreateFlatMesh(float min_depth, float max_depth,
                    PinholeCamera<float> &cam, int grid_size,
                    std::vector<float> &vertex,
                    std::vector<int> &indices,
                    bool add_pos = true,
                    bool add_tex = true,
                    bool add_normal = true)
{
    std::vector<Vec2<float>> grid_uv = UniformTexCoords(grid_size, grid_size);

    int stride = 0;
    if (add_pos)
        stride += 3;
    if (add_tex)
        stride += 2;
    if (add_normal)
        stride += 3;

    std::vector<Eigen::Vector2f> texcoords;
    texcoords.reserve(grid_uv.size());

    vertex.clear();
    vertex.reserve(grid_uv.size() * stride);

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

        texcoords.push_back(Eigen::Vector2f(uv(0), uv(1)));
    }

    BuildTriangles(texcoords, indices);
}

void CreateFlatMesh(float min_depth, float max_depth,
                    PinholeCamera<float> &cam, int grid_size,
                    std::vector<Eigen::Vector3f> &vertices,
                    std::vector<Eigen::Vector2f> &texcoords,
                    std::vector<int> &indices)
{
    std::vector<Vec2<float>> grid_uv = UniformTexCoords(grid_size, grid_size);

    vertices.clear();
    texcoords.clear();

    vertices.reserve(grid_uv.size());
    texcoords.reserve(grid_uv.size());

    for (const Vec2<float> &uv : grid_uv)
    {
        const float depth = VerticallySmoothDepth(uv, min_depth, max_depth);

        if (depth <= 0.0f)
            continue;

        const Vec3<float> ray = cam.PixToRay(uv);
        const Vec3<float> vertex = ray * depth;

        vertices.push_back(Eigen::Vector3f(vertex(0), vertex(1), vertex(2)));
        texcoords.push_back(Eigen::Vector2f(uv(0), uv(1)));
    }

    BuildTriangles(texcoords, indices);
}

void CreateSphereMesh(float depth,
                      PinholeCamera<float> &cam, int grid_size,
                      std::vector<Eigen::Vector3f> &vertices,
                      std::vector<Eigen::Vector2f> &texcoords,
                      std::vector<int> &indices)
{
    std::vector<Vec2<float>> grid_uv = UniformTexCoords(grid_size, grid_size);

    vertices.clear();
    texcoords.clear();

    vertices.reserve(grid_uv.size());
    texcoords.reserve(grid_uv.size());

    for (const Vec2<float> &uv : grid_uv)
    {
        if (depth <= 0.0f)
            continue;

        Vec3<float> ray = cam.PixToRay(uv);
        ray = ray / ray.norm();
        const Vec3<float> vertex = ray * depth;

        vertices.push_back(Eigen::Vector3f(vertex(0), vertex(1), vertex(2)));
        texcoords.push_back(Eigen::Vector2f(uv(0), uv(1)));
    }

    BuildTriangles(texcoords, indices);
}
