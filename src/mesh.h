#pragma once

#include "common/types.h"
#include "common/delaunaytriangulation.h"

class Mesh
{
public:

protected:
    std::vector<unsigned int> BuildTriangles(std::vector<float> tex_coords)
    {
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

    DelaunayTriangulation triangulator_;
};
