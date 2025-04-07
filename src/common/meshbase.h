#pragma once

#include "common/delaunaytriangulation.h"

class MeshBase
{
public:
    MeshBase(){}

protected:
    std::vector<Vec3i> BuildTriangles(std::vector<Vec2> tex_coords)
    {
        triangulator_.LoadPoints(tex_coords);
        triangulator_.Triangulate();
        std::vector<Vec3i> tris = triangulator_.GetTriangles();
        return tris;
    }

    DelaunayTriangulation triangulator_;
};
