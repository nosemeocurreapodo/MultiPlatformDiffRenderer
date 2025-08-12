#pragma once

#include <vector>
#include <array>
#include <cmath>

#include "backends/cpu/typescpu.h"
#include "core/common.h"

class DelaunayTriangulation
{
public:
    DelaunayTriangulation()
    {
    }

    void LoadPoints(std::vector<cpu::Vec2> texcoords)
    {
        vertices_ = texcoords;
    }

    void LoadTriangles(std::vector<cpu::Vec3i> &tris)
    {
        triangles_ = tris;
    }

    std::vector<cpu::Vec3i> GetTriangles()
    {
        return triangles_;
    }

    void TriangulateVertice(int v_id)
    {
        std::vector<cpu::Vec3i> goodTriangles;
        std::vector<cpu::Vec3i> badTriangles;
        std::vector<cpu::Vec2i> polygon;

        // check for bad triangles
        for (auto it = triangles_.begin(); it != triangles_.end(); ++it)
        {
            if (IsPointInCircumcircle(vertices_[v_id], *it))
                badTriangles.push_back(*it);
            else
                goodTriangles.push_back(*it);
        }

        for (const auto &tri : badTriangles)
        {
            std::array<cpu::Vec2i, 3> edges;
            //edges[0] = {tri(0), tri(1)};
            //edges[1] = {tri(1), tri(2)};
            //edges[2] = {tri(2), tri(0)};
            
            edges[0] = cpu::Vec2i(tri(0), tri(1));
            edges[1] = cpu::Vec2i(tri(1), tri(2));
            edges[2] = cpu::Vec2i(tri(2), tri(0));

            for (size_t j = 0; j < edges.size(); j++)
            {
                cpu::Vec2i edge = edges[j];
                int edge_index = -1;
                for (size_t k = 0; k < polygon.size(); k++)
                {
                    cpu::Vec2i pol = polygon[k];

                    if (IsEdgeEqual(edge, pol))
                    {
                        edge_index = k;
                        break;
                    }
                }

                if (edge_index >= 0)
                {
                    polygon.erase(polygon.begin() + edge_index);
                }
                else
                {
                    polygon.push_back(edge);
                }
            }
        }

        triangles_ = goodTriangles;

        for (const auto &edge : polygon)
        {
            cpu::Vec3i tri;
            tri(0) = edge(0);
            tri(1) = edge(1);
            tri(2) = v_id;

            /*
            Triangle2D triStru = Triangle2D(vertices[tri[0]], vertices[tri[1]], vertices[tri[2]]);
            float area = triStru.getArea();
            if(area <= 0)
            {
                tri[0] = edge[1];
                tri[1] = edge[0];
            }
            */

            triangles_.push_back(tri);
        }
    }

    void Triangulate()
    {
        triangles_.clear();
        std::array<cpu::Vec2, 3> superTriangleVertices = GetSuperTriangle();
        cpu::Vec3i superTriangleIndices;

        superTriangleIndices(0) = vertices_.size();
        vertices_.push_back(superTriangleVertices[0]);

        superTriangleIndices(1) = vertices_.size();
        vertices_.push_back(superTriangleVertices[1]);

        superTriangleIndices(2) = vertices_.size();
        vertices_.push_back(superTriangleVertices[2]);

        triangles_.push_back(superTriangleIndices);

        for (int it = 0; it < (int)vertices_.size() - 3; ++it)
        {
            TriangulateVertice(it);
        }

        RemoveVertice(superTriangleIndices(0));
        RemoveVertice(superTriangleIndices(1));
        RemoveVertice(superTriangleIndices(2));
    }

private:
    std::array<cpu::Vec2, 3> GetSuperTriangle()
    {
        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double maxY = std::numeric_limits<double>::lowest();

        for (auto it = vertices_.begin(); it != vertices_.end(); ++it)
        // for (const auto &point : points)
        {
            cpu::Vec2 point = *it;

            if (point(0) < minX)
                minX = point(0);
            if (point(1) < minY)
                minY = point(1);
            if (point(0) > maxX)
                maxX = point(0);
            if (point(1) > maxY)
                maxY = point(1);
        }

        double dx = maxX - minX;
        double dy = maxY - minY;
        double deltaMax = std::max(dx, dy);
        double midX = (minX + maxX) / 2;
        double midY = (minY + maxY) / 2;

        std::array<cpu::Vec2, 3> superTriangle;

        superTriangle[0] = cpu::Vec2(midX - 2 * deltaMax, midY - deltaMax);
        superTriangle[1] = cpu::Vec2(midX, midY + 2 * deltaMax);
        superTriangle[2] = cpu::Vec2(midX + 2 * deltaMax, midY - deltaMax);

        return superTriangle;
    }

    void RemoveVertice(int v_id)
    {
        // int superA = superTriangle[0];
        // int superB = superTriangle[1];
        // int superC = superTriangle[2];

        // triangles.erase(std::remove_if(triangles.begin(), triangles.end(), [&](const std::array<unsigned int, 3> &tri)
        //                                { return (tri[0] == superA || tri[0] == superB || tri[0] == superC ||
        //                                          tri[1] == superA || tri[1] == superB || tri[1] == superC ||
        //                                          tri[2] == superA || tri[2] == superB || tri[2] == superC); }),
        //                 triangles.end());

        vertices_.erase(vertices_.begin() + v_id);

        std::vector<int> to_remove;
        for (int it = 0; it < (int)triangles_.size(); it++)
        {
            cpu::Vec3i tri = triangles_[it];

            if (v_id == tri(0) || v_id == tri(1) || v_id == tri(2))
            {
                to_remove.push_back(it);
            }
        }

        if (to_remove.size() > 0)
        {
            for (int it = to_remove.size() - 1; it >= 0; it--)
            {
                triangles_.erase(triangles_.begin() + to_remove[it]);
            }
        }

        /*
        for (auto it = triangles.begin(); it != triangles.end();)
        {
            bool todelete = false;
            vec3<unsigned int> tri = *it;
            for (int i = 0; i < 3; i++)
            {
                if (tri(i) == v_id)
                {
                    todelete = true;
                    break;
                }
            }
            if (todelete)
                it = triangles.erase(it);
            else
                ++it;
        }
        */
    }

    bool IsPointInCircumcircle(cpu::Vec2 &point, cpu::Vec3i &tri)
    {
        std::pair<cpu::Vec2, double> center_radius = Circumcircle(tri);
        cpu::Vec2 circumcenter = center_radius.first;
        double circumradius = center_radius.second;
        double dist = std::sqrt((point(0) - circumcenter(0)) * (point(0) - circumcenter(0)) + (point(1) - circumcenter(1)) * (point(1) - circumcenter(1)));
        return dist <= circumradius;
    }
    std::pair<cpu::Vec2, double> Circumcircle(cpu::Vec3i &tri)
    {
        cpu::Vec2 A = vertices_[tri(0)];
        cpu::Vec2 B = vertices_[tri(1)];
        cpu::Vec2 C = vertices_[tri(2)];

        double D = 2 * (A(0) * (B(1) - C(1)) + B(0) * (C(1) - A(1)) + C(0) * (A(1) - B(1)));
        double Ux = ((A(0) * A(0) + A(1) * A(1)) * (B(1) - C(1)) + (B(0) * B(0) + B(1) * B(1)) * (C(1) - A(1)) + (C(0) * C(0) + C(1) * C(1)) * (A(1) - B(1))) / D;
        double Uy = ((A(0) * A(0) + A(1) * A(1)) * (C(0) - B(0)) + (B(0) * B(0) + B(1) * B(1)) * (A(0) - C(0)) + (C(0) * C(0) + C(1) * C(1)) * (B(0) - A(0))) / D;

        cpu::Vec2 circumcenter(Ux, Uy);
        double circumradius = std::sqrt((circumcenter(0) - A(0)) * (circumcenter(0) - A(0)) + (circumcenter(1) - A(1)) * (circumcenter(1) - A(1)));

        return {circumcenter, circumradius};
    }

    std::vector<cpu::Vec2> vertices_;
    std::vector<cpu::Vec3i> triangles_;
};

inline std::vector<unsigned int> BuildTriangles(std::vector<float> tex_coords)
{
    DelaunayTriangulation triangulator_;
    std::vector<cpu::Vec2> tex_coords_2d;
    for (size_t i = 0; i < tex_coords.size(); i += 2)
    {
        tex_coords_2d.push_back(cpu::Vec2(tex_coords[i], tex_coords[i + 1]));
    }
    triangulator_.LoadPoints(tex_coords_2d);
    triangulator_.Triangulate();
    std::vector<cpu::Vec3i> tris = triangulator_.GetTriangles();
    std::vector<unsigned int> tris_f;
    for (size_t i = 0; i < tris.size(); i++)
    {
        tris_f.push_back(tris[i](0));
        tris_f.push_back(tris[i](1));
        tris_f.push_back(tris[i](2));
    }
    return tris_f;
}
