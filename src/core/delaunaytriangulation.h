#pragma once

#include <vector>
#include <array>
#include <cmath>

#include "linalg/linalg.h"

class DelaunayTriangulation
{
public:
    DelaunayTriangulation()
    {
    }

    void LoadPoints(std::vector<linalg::Vec2<float>> texcoords)
    {
        vertices_ = texcoords;
    }

    void LoadTriangles(std::vector<linalg::Vec3<int>> &tris)
    {
        triangles_ = tris;
    }

    std::vector<linalg::Vec3<int>> GetTriangles()
    {
        return triangles_;
    }

    void TriangulateVertice(int v_id)
    {
        std::vector<linalg::Vec3<int>> goodTriangles;
        std::vector<linalg::Vec3<int>> badTriangles;
        std::vector<linalg::Vec2<int>> polygon;

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
            std::array<linalg::Vec2<int>, 3> edges;
            // edges[0] = {tri(0), tri(1)};
            // edges[1] = {tri(1), tri(2)};
            // edges[2] = {tri(2), tri(0)};

            edges[0] = linalg::Vec2<int>(tri(0), tri(1));
            edges[1] = linalg::Vec2<int>(tri(1), tri(2));
            edges[2] = linalg::Vec2<int>(tri(2), tri(0));

            for (size_t j = 0; j < edges.size(); j++)
            {
                linalg::Vec2<int> edge = edges[j];
                int edge_index = -1;
                for (size_t k = 0; k < polygon.size(); k++)
                {
                    linalg::Vec2<int> pol = polygon[k];

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
            linalg::Vec3<int> tri;
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
        std::array<linalg::Vec2<float>, 3> superTriangleVertices = GetSuperTriangle();
        linalg::Vec3<int> superTriangleIndices;

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
    bool IsTriangleEqual(linalg::Vec3<int> tri_indices_1, linalg::Vec3<int> tri_indices_2)
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

    bool IsEdgeEqual(linalg::Vec2<int> edge_indices_1, linalg::Vec2<int> edge_indices_2)
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
    std::array<linalg::Vec2<float>, 3> GetSuperTriangle()
    {
        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double maxY = std::numeric_limits<double>::lowest();

        for (auto it = vertices_.begin(); it != vertices_.end(); ++it)
        // for (const auto &point : points)
        {
            linalg::Vec2<float> point = *it;

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

        std::array<linalg::Vec2<float>, 3> superTriangle;

        superTriangle[0] = linalg::Vec2<float>(midX - 2 * deltaMax, midY - deltaMax);
        superTriangle[1] = linalg::Vec2<float>(midX, midY + 2 * deltaMax);
        superTriangle[2] = linalg::Vec2<float>(midX + 2 * deltaMax, midY - deltaMax);

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
            linalg::Vec3<int> tri = triangles_[it];

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

    bool IsPointInCircumcircle(linalg::Vec2<float> &point, linalg::Vec3<int> &tri)
    {
        std::pair<linalg::Vec2<float>, double> center_radius = Circumcircle(tri);
        linalg::Vec2<float> circumcenter = center_radius.first;
        double circumradius = center_radius.second;
        double dist = std::sqrt((point(0) - circumcenter(0)) * (point(0) - circumcenter(0)) + (point(1) - circumcenter(1)) * (point(1) - circumcenter(1)));
        return dist <= circumradius;
    }
    std::pair<linalg::Vec2<float>, double> Circumcircle(linalg::Vec3<int> &tri)
    {
        linalg::Vec2<float> A = vertices_[tri(0)];
        linalg::Vec2<float> B = vertices_[tri(1)];
        linalg::Vec2<float> C = vertices_[tri(2)];

        double D = 2 * (A(0) * (B(1) - C(1)) + B(0) * (C(1) - A(1)) + C(0) * (A(1) - B(1)));
        double Ux = ((A(0) * A(0) + A(1) * A(1)) * (B(1) - C(1)) + (B(0) * B(0) + B(1) * B(1)) * (C(1) - A(1)) + (C(0) * C(0) + C(1) * C(1)) * (A(1) - B(1))) / D;
        double Uy = ((A(0) * A(0) + A(1) * A(1)) * (C(0) - B(0)) + (B(0) * B(0) + B(1) * B(1)) * (A(0) - C(0)) + (C(0) * C(0) + C(1) * C(1)) * (B(0) - A(0))) / D;

        linalg::Vec2<float> circumcenter(Ux, Uy);
        double circumradius = std::sqrt((circumcenter(0) - A(0)) * (circumcenter(0) - A(0)) + (circumcenter(1) - A(1)) * (circumcenter(1) - A(1)));

        return {circumcenter, circumradius};
    }

    std::vector<linalg::Vec2<float>> vertices_;
    std::vector<linalg::Vec3<int>> triangles_;
};
