#pragma once

#include <vector>
#include <unordered_set>
#include "mpdr/common/types.h"

template <class Mesh>
static std::vector<float> get_depths(const Mesh &mesh)
{
    std::vector<float> depths;
    depths.reserve(mesh.vertex_count());

    if (mesh.pos_offset_ < 0)
        return depths;
    auto vertex_map = mesh.vertex_buffer_.MapRead();
    for (int i = 0; i < mesh.vertex_count(); i++)
    {
        float depth = vertex_map[i * mesh.stride_ + mesh.pos_offset_ + 2];
        depths.push_back(depth);
    }
    return depths;
}

template <class Mesh>
static std::vector<Vec3<float>> get_vertices(const Mesh &mesh)
{
    std::vector<Vec3<float>> vertices;
    vertices.reserve(mesh.vertex_count());

    if (mesh.pos_offset_ < 0)
        return vertices;
    auto vertex_map = mesh.vertex_buffer_.MapRead();
    for (int i = 0; i < mesh.vertex_count(); i++)
    {
        float v0 = vertex_map[i * mesh.stride_ + mesh.pos_offset_ + 0];
        float v1 = vertex_map[i * mesh.stride_ + mesh.pos_offset_ + 1];
        float v2 = vertex_map[i * mesh.stride_ + mesh.pos_offset_ + 2];

        Vec3<float> vertex(v0, v1, v2);
        vertices.push_back(vertex);
    }
    return vertices;
}

template <class Mesh>
static std::vector<Vec2<float>> get_texcoords(const Mesh &mesh)
{
    std::vector<Vec2<float>> texcoords;
    texcoords.reserve(mesh.vertex_count());

    if (mesh.tex_offset_ < 0)
        return texcoords;
    auto vertex_map = mesh.vertex_buffer_.MapRead();
    for (int i = 0; i < mesh.vertex_count(); i++)
    {
        float v0 = vertex_map[i * mesh.stride_ + mesh.tex_offset_ + 0];
        float v1 = vertex_map[i * mesh.stride_ + mesh.tex_offset_ + 1];

        Vec2<float> texcoord(v0, v1);
        texcoords.push_back(texcoord);
    }
    return texcoords;
}

template <class Mesh>
static std::vector<Vec3<int>> get_indices(const Mesh &mesh)
{
    std::vector<Vec3<int>> ids;
    auto index_map = mesh.ebo_buffer_.MapRead();
    for (int i = 0; i < mesh.triangle_count(); i++)
    {
        Vec3<int> id;
        id(0) = index_map[i * 3 + 0];
        id(1) = index_map[i * 3 + 1];
        id(2) = index_map[i * 3 + 2];

        ids.push_back(id);
    }
    return ids;
}

template <class Mesh>
static std::vector<Vec2<int>> get_edges(const Mesh &mesh)
{
    std::vector<Vec2<int>> edges;

    // If your mesh can be non-indexed, decide what "edges" means there.
    if (mesh.index_count() <= 0)
        return edges;

    // --- Read indices ---
    // Adjust this type if your index buffer is uint16_t, or add a small helper.
    auto idx = mesh.ebo_buffer_.MapRead(); // expected: pointer-like to indices
    // If MapRead() returns something like uint32_t*, you're good.

    // Track unique undirected edges using a 64-bit key (min,max)
    std::unordered_set<uint64_t> seen;
    seen.reserve(static_cast<size_t>(mesh.index_count())); // rough reserve

    auto add_edge = [&](uint32_t a, uint32_t b)
    {
        if (a == b)
            return; // ignore degenerate
        uint32_t lo = std::min(a, b);
        uint32_t hi = std::max(a, b);
        uint64_t key = (static_cast<uint64_t>(lo) << 32) | static_cast<uint64_t>(hi);

        if (seen.insert(key).second)
            edges.push_back(Vec2<int>(lo, hi));
    };

    // Triangles assumed: (i0,i1,i2)
    for (int i = 0; i + 2 < mesh.index_count(); i += 3)
    {
        uint32_t i0 = static_cast<uint32_t>(idx[i + 0]);
        uint32_t i1 = static_cast<uint32_t>(idx[i + 1]);
        uint32_t i2 = static_cast<uint32_t>(idx[i + 2]);

        add_edge(i0, i1);
        add_edge(i1, i2);
        add_edge(i2, i0);
    }

    return edges;
}

template <class Mesh>
static void set_depths(Mesh &mesh, const std::vector<float> &depths)
{
    auto vertex_map = mesh.vertex_buffer_.MapWrite();
    for (int i = 0; i < mesh.vertex_count(); i++)
    {
        Vec3<float> vertex;
        vertex(0) = vertex_map[i * mesh.stride_ + mesh.pos_offset_ + 0];
        vertex(1) = vertex_map[i * mesh.stride_ + mesh.pos_offset_ + 1];
        vertex(2) = vertex_map[i * mesh.stride_ + mesh.pos_offset_ + 2];

        Vec3<float> new_vertex = depths[i] * vertex / vertex(2);

        vertex_map[i * mesh.stride_ + mesh.pos_offset_ + 0] = new_vertex(0);
        vertex_map[i * mesh.stride_ + mesh.pos_offset_ + 1] = new_vertex(1);
        vertex_map[i * mesh.stride_ + mesh.pos_offset_ + 2] = new_vertex(2);
    }
}

template <class Mesh>
static void set_vertices(Mesh &mesh, const std::vector<Vec3<float>> &vertices)
{
    auto vertex_map = mesh.vertex_buffer_.MapWrite();
    for (int i = 0; i < mesh.vertex_count(); i++)
    {
        vertex_map[i * mesh.stride_ + mesh.pos_offset_ + 0] = vertices[i](0);
        vertex_map[i * mesh.stride_ + mesh.pos_offset_ + 1] = vertices[i](1);
        vertex_map[i * mesh.stride_ + mesh.pos_offset_ + 2] = vertices[i](2);
    }
}

template <class Mesh>
static void set_texcoords(Mesh &mesh, const std::vector<Vec2<float>> &texcoords)
{
    auto vertex_map = mesh.vertex_buffer_.MapWrite();
    for (int i = 0; i < mesh.vertex_count(); i++)
    {
        vertex_map[i * mesh.stride_ + mesh.tex_offset_ + 0] = texcoords[i](0);
        vertex_map[i * mesh.stride_ + mesh.tex_offset_ + 1] = texcoords[i](1);
    }
}