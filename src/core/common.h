#pragma once

static bool IsTriangleEqual(cpu::Vec3i tri_indices_1, cpu::Vec3i tri_indices_2)
{
    bool isIndicePresent[3];
    for (int tri_indice = 0; tri_indice < 3; tri_indice++)
    {
        isIndicePresent[tri_indice] = false;
        if (tri_indices_1(tri_indice) == tri_indices_2(0) || tri_indices_1(tri_indice) == tri_indices_2(1) || tri_indices_1(tri_indice) == tri_indices_2(2))
            isIndicePresent[tri_indice] = true;
    }
    if (isIndicePresent[0] && isIndicePresent[1] && isIndicePresent[2])
        return true;
    return false;
}

static bool IsEdgeEqual(cpu::Vec2i edge_indices_1, cpu::Vec2i edge_indices_2)
{
    bool isIndicePresent[2];
    for (int edge_indice = 0; edge_indice < 2; edge_indice++)
    {
        isIndicePresent[edge_indice] = false;
        if (edge_indices_1(edge_indice) == edge_indices_2(0) || edge_indices_1(edge_indice) == edge_indices_2(1))
            isIndicePresent[edge_indice] = true;
    }
    if (isIndicePresent[0] && isIndicePresent[1])
        return true;
    return false;
}

inline std::vector<cpu::Vec2> UniformTexCoords(int width, int height)
{
    std::vector<cpu::Vec2> texcoords;
    for (float y = 0.0; y < height; y++)
    {
        for (float x = 0.0; x < width; x++)
        {
            cpu::Vec2 pix;
            pix(0) = x / (width - 1);
            pix(1) = y / (height - 1);

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

inline float VerticallySmoothDepth(cpu::Vec2 pix, float min_depth, float max_depth)
{
    // max depth when y = 0
    float depth = max_depth + (min_depth - max_depth) * pix(1);
    return depth;
}
