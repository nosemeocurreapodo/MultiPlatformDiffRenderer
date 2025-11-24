#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <utility>

#include "core/types.h"

class DelaunayTriangulation
{
public:
    using Vec2f = Vec2<float>;
    using Tri3i = Vec3<int>;

    DelaunayTriangulation() = default;

    // Replace existing points (copy)
    void LoadPoints(const std::vector<Vec2f> &texcoords) { vertices_ = texcoords; }
    // Replace existing points (move)
    void LoadPoints(std::vector<Vec2f> &&texcoords) { vertices_ = std::move(texcoords); }

    // Optional: preload triangles (e.g., for debugging)
    void LoadTriangles(const std::vector<Tri3i> &tris) { triangles_ = tris; }
    void Clear()
    {
        vertices_.clear();
        triangles_.clear();
    }

    const std::vector<Tri3i> &GetTriangles() const { return triangles_; }
    const std::vector<Vec2f> &GetVertices() const { return vertices_; }

    void SetEpsilon(double eps) { eps2_ = eps * eps; }

    // Build a fresh triangulation from the current vertices
    void Triangulate()
    {
        triangles_.clear();
        if (vertices_.size() < 3)
            return;

        // Remember original size; we'll append 3 super-verts and then drop them in one go.
        const std::size_t base = vertices_.size();

        // Reserve rough triangle count ~ 2n
        triangles_.reserve(static_cast<std::size_t>(2 * base + 8));

        // Build & append super triangle
        auto superVerts = ComputeSuperTriangle();
        Tri3i superIdx;
        superIdx(0) = static_cast<int>(vertices_.size());
        vertices_.push_back(superVerts[0]);
        superIdx(1) = static_cast<int>(vertices_.size());
        vertices_.push_back(superVerts[1]);
        superIdx(2) = static_cast<int>(vertices_.size());
        vertices_.push_back(superVerts[2]);
        triangles_.push_back(superIdx);

        // Incrementally insert each original vertex
        for (int vid = 0; vid < static_cast<int>(base); ++vid)
        {
            InsertVertex(vid);
        }

        // Remove any triangle touching any of the 3 appended super-vertices,
        // then shrink vertices_ back to the original size.
        RemoveSuperTriangle(base);
    }

private:
    struct Edge
    {
        int a, b; // canonical: a < b
        Edge() : a(0), b(0) {}
        Edge(int u, int v)
        {
            if (u < v)
            {
                a = u;
                b = v;
            }
            else
            {
                a = v;
                b = u;
            }
        }
        bool operator==(const Edge &o) const { return a == o.a && b == o.b; }
    };

    struct EdgeHash
    {
        std::size_t operator()(const Edge &e) const noexcept
        {
            // Simple hash combine on two ints
            return std::hash<int>{}(e.a) * 73856093u ^ std::hash<int>{}(e.b) * 19349663u;
        }
    };

    // Insert one vertex via Bowyer–Watson
    void InsertVertex(int v_id)
    {
        std::vector<Tri3i> goodTriangles;
        goodTriangles.reserve(triangles_.size());

        // Track edges of "bad" triangles; boundary edges appear exactly once.
        std::unordered_map<Edge, int, EdgeHash> edgeCount;
        edgeCount.reserve(triangles_.size() * 3);

        // 1) Split triangles into "bad" (circumcircle contains point) and "good".
        for (const auto &tri : triangles_)
        {
            const int i0 = tri(0), i1 = tri(1), i2 = tri(2);
            if (InCircumcircle(i0, i1, i2, v_id))
            {
                // Add its edges to the edgeCount map
                ++edgeCount[Edge(i0, i1)];
                ++edgeCount[Edge(i1, i2)];
                ++edgeCount[Edge(i2, i0)];
            }
            else
            {
                goodTriangles.push_back(tri);
            }
        }

        // 2) Boundary polygon = edges seen exactly once.
        //    Triangulate each boundary edge with the new point.
        for (const auto &kv : edgeCount)
        {
            if (kv.second != 1)
                continue; // interior edges appear twice, ignore
            const Edge &e = kv.first;

            // Skip degenerate triangles (any duplicate index)
            if (e.a == v_id || e.b == v_id)
                continue;

            Tri3i newTri;
            newTri(0) = e.a;
            newTri(1) = e.b;
            newTri(2) = v_id;

            // Enforce CCW orientation (negative area (opecv coordinate system))
            if (!IsCCW(newTri))
            {
                std::swap(newTri(0), newTri(1));
            }

            // Still guard against near-degenerate area
            if (Area2(newTri) > eps2_)
            {
                goodTriangles.push_back(newTri);
            }
        }

        triangles_.swap(goodTriangles);
    }

    // Robust-ish orientation test; positive if CCW
    static double Orient(const Vec2f &a, const Vec2f &b, const Vec2f &c)
    {
        const double ax = a(0), ay = a(1);
        const double bx = b(0), by = b(1);
        const double cx = c(0), cy = c(1);
        return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
        // Vec2f ba = b - a;
        // Vec2f ca = c - a;
        // return ba.cross(ca);
    }

    bool IsCCW(const Tri3i &t) const
    {
        // check negative area (because opencv coordinate system)
        return Orient(vertices_[t(0)], vertices_[t(1)], vertices_[t(2)]) < 0.0;
    }

    double Area2(const Tri3i &t) const
    {
        return std::abs(Orient(vertices_[t(0)], vertices_[t(1)], vertices_[t(2)]));
    }

    // Robust incircle predicate (sign-corrected for triangle orientation).
    // Returns true if p is strictly inside the circumcircle of (i,j,k).
    bool InCircumcircle(int i, int j, int k, int p) const
    {
        const Vec2f &A = vertices_[i];
        const Vec2f &B = vertices_[j];
        const Vec2f &C = vertices_[k];
        const Vec2f &P = vertices_[p];

        // Translate so P is at the origin
        const double ax = A(0) - P(0), ay = A(1) - P(1);
        const double bx = B(0) - P(0), by = B(1) - P(1);
        const double cx = C(0) - P(0), cy = C(1) - P(1);

        const double a2 = ax * ax + ay * ay;
        const double b2 = bx * bx + by * by;
        const double c2 = cx * cx + cy * cy;

        // 2x2 minors
        const double ab = ax * by - ay * bx;
        const double bc = bx * cy - by * cx;
        const double ca = cx * ay - cy * ax;

        // Determinant of the 3x3 incircle matrix (after translation by P)
        double det = a2 * bc + b2 * ca + c2 * ab;

        // Correct sign according to triangle orientation:
        // For CCW (A,B,C), det > 0 means P is inside.
        const double orient = Orient(A, B, C);
        if (orient < 0.0)
            det = -det;

        // To avoid floating noise, use a small tolerance. We want "strictly inside".
        return det > incircle_tol_;
    }

    // Build a sufficiently large super-triangle around current vertices_
    std::array<Vec2f, 3> ComputeSuperTriangle() const
    {
        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double maxY = std::numeric_limits<double>::lowest();

        for (const auto &p : vertices_)
        {
            minX = std::min(minX, static_cast<double>(p(0)));
            minY = std::min(minY, static_cast<double>(p(1)));
            maxX = std::max(maxX, static_cast<double>(p(0)));
            maxY = std::max(maxY, static_cast<double>(p(1)));
        }

        const double dx = maxX - minX;
        const double dy = maxY - minY;
        const double deltaMax = std::max(dx, dy);
        const double midX = (minX + maxX) * 0.5;
        const double midY = (minY + maxY) * 0.5;

        // Bigger than your original to reduce edge effects
        const double R = 4.0 * deltaMax + 1.0;

        std::array<Vec2f, 3> st;
        st[0] = Vec2f(static_cast<float>(midX - 2.0 * R), static_cast<float>(midY - R));
        st[1] = Vec2f(static_cast<float>(midX), static_cast<float>(midY + 2.0 * R));
        st[2] = Vec2f(static_cast<float>(midX + 2.0 * R), static_cast<float>(midY - R));
        return st;
    }

    // Remove any triangles that reference the appended super-verts [base, base+2], then pop them
    void RemoveSuperTriangle(std::size_t base)
    {
        triangles_.erase(
            std::remove_if(triangles_.begin(), triangles_.end(),
                           [base](const Tri3i &t)
                           {
                               return (t(0) >= static_cast<int>(base)) ||
                                      (t(1) >= static_cast<int>(base)) ||
                                      (t(2) >= static_cast<int>(base));
                           }),
            triangles_.end());
        vertices_.resize(base);
    }

private:
    std::vector<Vec2f> vertices_;
    std::vector<Tri3i> triangles_;
    double eps2_ = 1e-14;         // area^2-ish tolerance for degeneracy checks
    double incircle_tol_ = 1e-18; // tolerance for incircle determinant
};
