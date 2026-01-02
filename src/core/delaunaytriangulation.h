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
    void LoadTriangles(const std::vector<Tri3i> &tris)
    {
        triangles_ = tris;
        NormalizeTriangleWinding();
    }
    void LoadTriangles(std::vector<Tri3i> &&tris)
    {
        triangles_ = std::move(tris);
        NormalizeTriangleWinding();
    }

    // NEW: load an already-triangulated mesh (verts + tris)
    void LoadMesh(const std::vector<Vec2f> &verts, const std::vector<Tri3i> &tris)
    {
        vertices_ = verts;
        triangles_ = tris;
        NormalizeTriangleWinding();
    }
    void LoadMesh(std::vector<Vec2f> &&verts, std::vector<Tri3i> &&tris)
    {
        vertices_ = std::move(verts);
        triangles_ = std::move(tris);
        NormalizeTriangleWinding();
    }

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

        NormalizeTriangleWinding();
    }

    // ============================================================
    // NEW: Incrementally add points that lie outside the current hull
    //      WITHOUT rebuilding from scratch.
    //
    // This does:
    //   - find visible chain on current convex hull
    //   - stitch a fan of triangles to the new point
    //   - locally restore Delaunay via Lawson edge flips
    //
    // Precondition: triangles_ is a valid triangulation of vertices_.
    // ============================================================

    // Add one outside point. Returns true if treated as outside-hull insertion.
    // If it returns false, the point was likely inside-hull / degenerate; you can
    // optionally fall back to InsertVertex (but that will remove/replace triangles).
    bool AddOutsidePoint(const Vec2f &p)
    {
        if (vertices_.size() < 3 || triangles_.empty())
        {
            vertices_.push_back(p);
            Triangulate();
            return true;
        }

        const int pid = static_cast<int>(vertices_.size());
        vertices_.push_back(p);

        // Build adjacency (edge -> up to 2 incident triangles)
        auto adj = BuildAdjacency();

        // Find visible hull chain from pid
        std::vector<int> chain = ComputeVisibleHullChain(pid, adj);
        if (chain.size() < 2)
            return false; // probably not outside hull (or numerically borderline)

        // Add fan triangles along chain edges (no deletions)
        std::vector<Edge> legalize;
        legalize.reserve(chain.size() * 4);

        for (std::size_t i = 0; i + 1 < chain.size(); ++i)
        {
            const int a = chain[i];
            const int b = chain[i + 1];

            Tri3i t;
            t(0) = a;
            t(1) = b;
            t(2) = pid;

            if (!IsCCW(t))
                std::swap(t(0), t(1));

            if (Area2(t) <= eps2_)
                continue;

            const int tIdx = static_cast<int>(triangles_.size());
            triangles_.push_back(t);
            AddTriToAdj(adj, tIdx);

            // The former hull edge (a,b) becomes interior -> may be illegal now
            legalize.push_back(Edge(a, b));
        }

        // Local Delaunay restoration
        LegalizeEdges(legalize, adj);

        return true;
    }

    // Add many outside points (processed sequentially; hull updates each insertion)
    void AddOutsidePoints(const std::vector<Vec2f> &pts)
    {
        for (const auto &p : pts)
            AddOutsidePoint(p);
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
            return std::hash<int>{}(e.a) * 73856093u ^ std::hash<int>{}(e.b) * 19349663u;
        }
    };

    // For adjacency: each undirected edge has up to 2 incident triangles
    struct EdgeAdj
    {
        int t0 = -1;
        int t1 = -1;
    };

    // Insert one vertex via Bowyer–Watson (used by Triangulate / optional fallback)
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
        for (const auto &kv : edgeCount)
        {
            if (kv.second != 1)
                continue;

            const Edge &e = kv.first;
            if (e.a == v_id || e.b == v_id)
                continue;

            Tri3i newTri;
            newTri(0) = e.a;
            newTri(1) = e.b;
            newTri(2) = v_id;

            if (!IsCCW(newTri))
                std::swap(newTri(0), newTri(1));

            if (Area2(newTri) > eps2_)
                goodTriangles.push_back(newTri);
        }

        triangles_.swap(goodTriangles);
    }

    // Robust-ish orientation test; positive if CCW (math coords)
    static double Orient(const Vec2f &a, const Vec2f &b, const Vec2f &c)
    {
        const double ax = a(0), ay = a(1);
        const double bx = b(0), by = b(1);
        const double cx = c(0), cy = c(1);
        return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
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

    void NormalizeTriangleWinding()
    {
        for (auto &t : triangles_)
        {
            if (!IsCCW(t))
                std::swap(t(0), t(1));
        }
    }

    // Robust incircle predicate (sign-corrected for triangle orientation).
    bool InCircumcircle(int i, int j, int k, int p) const
    {
        const Vec2f &A = vertices_[i];
        const Vec2f &B = vertices_[j];
        const Vec2f &C = vertices_[k];
        const Vec2f &P = vertices_[p];

        const double ax = A(0) - P(0), ay = A(1) - P(1);
        const double bx = B(0) - P(0), by = B(1) - P(1);
        const double cx = C(0) - P(0), cy = C(1) - P(1);

        const double a2 = ax * ax + ay * ay;
        const double b2 = bx * bx + by * by;
        const double c2 = cx * cx + cy * cy;

        const double ab = ax * by - ay * bx;
        const double bc = bx * cy - by * cx;
        const double ca = cx * ay - cy * ax;

        double det = a2 * bc + b2 * ca + c2 * ab;

        const double orient = Orient(A, B, C);
        if (orient < 0.0)
            det = -det;

        return det > incircle_tol_;
    }

    // --------------------------
    // Outside-hull insertion helpers
    // --------------------------

    static int OppositeVertex(const Tri3i &t, int u, int v)
    {
        const int a = t(0), b = t(1), c = t(2);
        if ((a == u && b == v) || (a == v && b == u))
            return c;
        if ((b == u && c == v) || (b == v && c == u))
            return a;
        if ((c == u && a == v) || (c == v && a == u))
            return b;
        return -1;
    }

    // edge uv is visible from point p if p is on opposite side of uv compared to triangle interior vertex opp
    bool IsEdgeVisibleFromPoint(int u, int v, int opp, int p) const
    {
        const auto &U = vertices_[u];
        const auto &V = vertices_[v];
        const auto &O = vertices_[opp];
        const auto &P = vertices_[p];

        const double s_in = Orient(U, V, O);
        const double s_p = Orient(U, V, P);

        if (std::abs(s_in) < 1e-30 || std::abs(s_p) < 1e-30)
            return false;

        return (s_in * s_p) < 0.0;
    }

    std::unordered_map<Edge, EdgeAdj, EdgeHash> BuildAdjacency() const
    {
        std::unordered_map<Edge, EdgeAdj, EdgeHash> adj;
        adj.reserve(triangles_.size() * 3);

        for (int ti = 0; ti < static_cast<int>(triangles_.size()); ++ti)
        {
            const auto &t = triangles_[ti];

            auto &a01 = adj[Edge(t(0), t(1))];
            if (a01.t0 == -1)
                a01.t0 = ti;
            else
                a01.t1 = ti;

            auto &a12 = adj[Edge(t(1), t(2))];
            if (a12.t0 == -1)
                a12.t0 = ti;
            else
                a12.t1 = ti;

            auto &a20 = adj[Edge(t(2), t(0))];
            if (a20.t0 == -1)
                a20.t0 = ti;
            else
                a20.t1 = ti;
        }

        return adj;
    }

    void AddEdgeRef(std::unordered_map<Edge, EdgeAdj, EdgeHash> &adj, const Edge &e, int t)
    {
        auto &a = adj[e];
        if (a.t0 == -1)
            a.t0 = t;
        else if (a.t1 == -1)
            a.t1 = t;
        // else non-manifold; ignore or assert in debug
    }

    void RemoveEdgeRef(std::unordered_map<Edge, EdgeAdj, EdgeHash> &adj, const Edge &e, int t)
    {
        auto it = adj.find(e);
        if (it == adj.end())
            return;

        auto &a = it->second;
        if (a.t0 == t)
            a.t0 = -1;
        else if (a.t1 == t)
            a.t1 = -1;

        if (a.t0 == -1 && a.t1 != -1)
            std::swap(a.t0, a.t1);

        if (a.t0 == -1 && a.t1 == -1)
            adj.erase(it);
    }

    void AddTriToAdj(std::unordered_map<Edge, EdgeAdj, EdgeHash> &adj, int tIdx)
    {
        const auto &t = triangles_[tIdx];
        AddEdgeRef(adj, Edge(t(0), t(1)), tIdx);
        AddEdgeRef(adj, Edge(t(1), t(2)), tIdx);
        AddEdgeRef(adj, Edge(t(2), t(0)), tIdx);
    }

    void RemoveTriFromAdj(std::unordered_map<Edge, EdgeAdj, EdgeHash> &adj, int tIdx)
    {
        const auto &t = triangles_[tIdx];
        RemoveEdgeRef(adj, Edge(t(0), t(1)), tIdx);
        RemoveEdgeRef(adj, Edge(t(1), t(2)), tIdx);
        RemoveEdgeRef(adj, Edge(t(2), t(0)), tIdx);
    }

    // Returns ordered chain of hull vertices v0..vk such that edges (v_i, v_{i+1}) are visible from p
    std::vector<int> ComputeVisibleHullChain(int p, const std::unordered_map<Edge, EdgeAdj, EdgeHash> &adj) const
    {
        // visible boundary edge graph (should form a path on the convex hull)
        std::unordered_map<int, std::vector<int>> nbrs;
        nbrs.reserve(64);

        int any = -1;

        for (const auto &kv : adj)
        {
            const Edge &e = kv.first;
            const EdgeAdj &ea = kv.second;

            // Boundary edge => exactly one triangle
            const int tri = (ea.t0 != -1 && ea.t1 == -1) ? ea.t0 : ((ea.t1 != -1 && ea.t0 == -1) ? ea.t1 : -1);
            if (tri == -1)
                continue;

            const int opp = OppositeVertex(triangles_[tri], e.a, e.b);
            if (opp < 0)
                continue;

            if (!IsEdgeVisibleFromPoint(e.a, e.b, opp, p))
                continue;

            nbrs[e.a].push_back(e.b);
            nbrs[e.b].push_back(e.a);
            any = e.a;
        }

        if (any == -1)
            return {};

        // find an endpoint (degree 1)
        int start = -1;
        for (const auto &kv : nbrs)
        {
            if (kv.second.size() == 1)
            {
                start = kv.first;
                break;
            }
        }
        if (start == -1)
            return {}; // cycle/degenerate

        // walk the chain
        std::vector<int> chain;
        chain.reserve(nbrs.size() + 1);

        int prev = -1;
        int cur = start;
        chain.push_back(cur);

        while (true)
        {
            const auto &ns = nbrs[cur];
            int next = -1;

            if (ns.size() == 1)
                next = ns[0];
            else
                next = (ns[0] != prev) ? ns[0] : ns[1];

            chain.push_back(next);
            prev = cur;
            cur = next;

            if (nbrs[cur].size() == 1)
                break;
        }

        return chain;
    }

    void FlipEdge(const Edge &e, int tA, int tB, std::unordered_map<Edge, EdgeAdj, EdgeHash> &adj)
    {
        const int u = e.a;
        const int v = e.b;

        const Tri3i oldA = triangles_[tA];
        const Tri3i oldB = triangles_[tB];

        const int w = OppositeVertex(oldA, u, v);
        const int x = OppositeVertex(oldB, u, v);
        if (w < 0 || x < 0 || w == x)
            return;

        // Avoid making a non-manifold edge (w,x) with already 2 incident tris
        auto it = adj.find(Edge(w, x));
        if (it != adj.end() && it->second.t0 != -1 && it->second.t1 != -1)
            return;

        RemoveTriFromAdj(adj, tA);
        RemoveTriFromAdj(adj, tB);

        Tri3i nA;
        nA(0) = w;
        nA(1) = x;
        nA(2) = u;

        Tri3i nB;
        nB(0) = x;
        nB(1) = w;
        nB(2) = v;

        if (!IsCCW(nA))
            std::swap(nA(0), nA(1));
        if (!IsCCW(nB))
            std::swap(nB(0), nB(1));

        if (Area2(nA) <= eps2_ || Area2(nB) <= eps2_)
        {
            // restore old adjacency if we bail
            triangles_[tA] = oldA;
            triangles_[tB] = oldB;
            AddTriToAdj(adj, tA);
            AddTriToAdj(adj, tB);
            return;
        }

        triangles_[tA] = nA;
        triangles_[tB] = nB;

        AddTriToAdj(adj, tA);
        AddTriToAdj(adj, tB);
    }

    void LegalizeEdges(std::vector<Edge> &stack, std::unordered_map<Edge, EdgeAdj, EdgeHash> &adj)
    {
        while (!stack.empty())
        {
            const Edge e = stack.back();
            stack.pop_back();

            auto it = adj.find(e);
            if (it == adj.end())
                continue;

            const EdgeAdj ea = it->second;
            if (ea.t0 == -1 || ea.t1 == -1)
                continue; // boundary edge

            const int tA = ea.t0;
            const int tB = ea.t1;

            const Tri3i &TA = triangles_[tA];
            const Tri3i &TB = triangles_[tB];

            const int u = e.a;
            const int v = e.b;

            const int x = OppositeVertex(TB, u, v);
            const int w = OppositeVertex(TA, u, v);
            if (x < 0 || w < 0)
                continue;

            // If x is inside circumcircle of TA, edge (u,v) is illegal -> flip
            if (InCircumcircle(TA(0), TA(1), TA(2), x))
            {
                FlipEdge(e, tA, tB, adj);

                // After flipping, neighboring edges can become illegal too
                stack.push_back(Edge(w, u));
                stack.push_back(Edge(u, x));
                stack.push_back(Edge(x, v));
                stack.push_back(Edge(v, w));
            }
        }
    }

    // --------------------------
    // Super triangle (existing)
    // --------------------------
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

        const double R = 4.0 * deltaMax + 1.0;

        std::array<Vec2f, 3> st;
        st[0] = Vec2f(static_cast<float>(midX - 2.0 * R), static_cast<float>(midY - R));
        st[1] = Vec2f(static_cast<float>(midX), static_cast<float>(midY + 2.0 * R));
        st[2] = Vec2f(static_cast<float>(midX + 2.0 * R), static_cast<float>(midY - R));
        return st;
    }

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
