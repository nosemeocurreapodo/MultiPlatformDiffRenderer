#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>

#include "backends/cpu/buffercpu.h"
#include "core/delaunaytriangulation.h"

class MeshCPU
{
public:
    using index_type = std::uint32_t;
    // using size_type = std::size_t;

    // Default: screen-aligned quad (two tris), z = 1
    /*
    MeshCPU()
    {
        // 6 vertices (two triangles), pos as (x,y,z)
        static constexpr float kPos[] = {
            -1.f, 1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f, 1.f,
            -1.f, 1.f, 1.f, 1.f, -1.f, 1.f, 1.f, 1.f, 1.f};
        static constexpr float kUV[] = {
            0.f, 1.f, 0.f, 0.f, 1.f, 0.f,
            0.f, 1.f, 1.f, 0.f, 1.f, 1.f};
        static constexpr float kW[] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f};

        pos_buffer_ = BufferCPU<float>(std::size(kPos), kPos);
        tex_buffer_ = BufferCPU<float>(std::size(kUV), kUV);
        wei_buffer_ = BufferCPU<float>(std::size(kW), kW);

        // Indices for the 6-vertex list above
        static constexpr index_type kIdx[] = {0, 1, 2, 3, 4, 5};
        ebo_buffer_ = BufferCPU<index_type>(std::size(kIdx), kIdx);

        validate_();
    }
    */

    // Construct from host vectors; if indices empty, build via Delaunay on UVs
    MeshCPU(const std::vector<float> &positions, // 3 floats per vertex
            const std::vector<float> &texcoords, // 2 floats per vertex
            const std::vector<float> &weights,   // 1 float  per vertex
            const std::vector<index_type> &indices = {})
        : pos_buffer_(positions),
          tex_buffer_(texcoords),
          wei_buffer_(weights)
    {
        if (!indices.empty())
        {
            ebo_buffer_ = BufferCPU<index_type>(indices);
        }
        else
        {
            // Delaunay returns unsigned int; convert to std::size_t
            std::vector<unsigned int> tris = BuildTriangles(texcoords);
            std::vector<index_type> idx(tris.begin(), tris.end());
            ebo_buffer_ = BufferCPU<index_type>(idx);
        }
        validate_();
    }

    // Copy/move
    MeshCPU(const MeshCPU &) = default;
    MeshCPU &operator=(const MeshCPU &) = default;
    MeshCPU(MeshCPU &&) noexcept = default;
    MeshCPU &operator=(MeshCPU &&) noexcept = default;
    ~MeshCPU() = default;

    /*
    // Read-only access to buffers (keeps ownership internal)
    const BufferCPU<float> &positions() const noexcept { return pos_buffer_; }
    const BufferCPU<float> &texcoords() const noexcept { return tex_buffer_; }
    const BufferCPU<float> &weights() const noexcept { return wei_buffer_; }
    const BufferCPU<index_type> &indices() const noexcept { return ebo_buffer_; }
    */

    // Cross-backend style mapped views (avoid storing the view)
    [[nodiscard]] MappedView<const float> MapReadPositions() const & { return pos_buffer_.MapRead(); }
    [[nodiscard]] MappedView<const float> MapReadTexcoords() const & { return tex_buffer_.MapRead(); }
    [[nodiscard]] MappedView<const float> MapReadWeights() const & { return wei_buffer_.MapRead(); }
    [[nodiscard]] MappedView<const index_type> MapReadIndices() const & { return ebo_buffer_.MapRead(); }

    [[nodiscard]] MappedView<float> MapWritePositions() { return pos_buffer_.MapWrite(); }
    [[nodiscard]] MappedView<float> MapWriteTexcoords() { return tex_buffer_.MapWrite(); }
    [[nodiscard]] MappedView<float> MapWriteWeights() { return wei_buffer_.MapWrite(); }
    [[nodiscard]] MappedView<index_type> MapWriteIndices() { return ebo_buffer_.MapWrite(); }

    // Info
    std::size_t vertex_count() const noexcept { return pos_buffer_.size() / 3; }
    std::size_t index_count() const noexcept { return ebo_buffer_.size(); }
    std::size_t triangle_count() const noexcept { return index_count() / 3; }

    /*
    // Rebuild indices from current texcoords (e.g., after UV edit)
    void rebuild_indices_from_uv()
    {
        //std::vector<unsigned int> tris = BuildTriangles(
        //    std::vector<float>(tex_buffer_.begin(), tex_buffer_.end()));

        auto m = tex_buffer_.MapRead();
        std::vector<unsigned int> tris = BuildTriangles(
            std::vector<float>(m.begin(), m.end()));

        std::vector<index_type> idx(tris.begin(), tris.end());
        ebo_buffer_ = BufferCPU<index_type>(idx);
        assert(index_count() % 3 == 0);
    }
    */

private:
    void validate_() const
    {
        // position size must be multiple of 3
        assert(pos_buffer_.size() % 3 == 0);
        const std::size_t nverts = pos_buffer_.size() / 3;

        // tex must be multiple of 2 and match vertex count
        assert(tex_buffer_.size() % 2 == 0);
        assert(tex_buffer_.size() / 2 == nverts);

        // weights match vertex count
        assert(wei_buffer_.size() == nverts);

        // indices multiple of 3
        assert(ebo_buffer_.size() % 3 == 0);

        // (Optional) indices range check in debug
#ifndef NDEBUG
        auto idx = ebo_buffer_.MapRead();
        for (std::size_t i = 0; i < idx.size(); ++i)
        {
            assert(idx[i] < static_cast<std::size_t>(nverts));
        }
#endif
    }

    BufferCPU<float> pos_buffer_;
    BufferCPU<float> tex_buffer_;
    BufferCPU<float> wei_buffer_;
    BufferCPU<index_type> ebo_buffer_;
};

inline MeshCPU CreateMesh(const TextureCPU<float> &depth, Camera &cam, int grid_size)
{
    std::vector<Vec2> grid_uv = UniformTexCoords(grid_size, grid_size);

    std::vector<float> vertices, texcoords, weights;

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

    return MeshCPU(vertices, texcoords, weights);
}
