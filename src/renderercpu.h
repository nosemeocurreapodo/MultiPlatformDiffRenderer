#pragma once

#include "common/types.h"
#include "common/devicecpu.h"
#include "common/texturecpu.h"
#include "common/buffercpu.h"
#include "common/meshcpu.h"

/*
struct Renderbuffer { int w, h, ys; void *data; };
struct Vert { vec4 position, texcoord, color; };
struct Varying { vec4 texcoord, color; };

void vertex_shader(const Vert &in, vec4 &gl_Position, Varying &OUT) {
    OUT.texcoord = in.texcoord;
    OUT.color = in.color;
    gl_Position = vec4(in.position.x, in.position.y, -2*in.position.z - 2*in.position.w, -in.position.z);
}

void fragment_shader(vec4 &gl_FragCoord, const Varying &IN, vec4 &OUT) {
    OUT = IN.color;
    vec2 wrapped = IN.texcoord.xy - floor(IN.texcoord.xy);
    bool brighter = (wrapped[0] < 0.5) != (wrapped[1] < 0.5);
    if(!brighter)
        OUT.rgb *= 0.5f;
}

// render output unit/render operations pipeline
void rop(Renderbuffer &buf, int x, int y, const vec4 &c) {
    uint8_t *p = (uint8_t*)buf.data + buf.ys*(buf.h - y - 1) + 4*x;
    p[0] = linear_to_srgb8(c[0]);
    p[1] = linear_to_srgb8(c[1]);
    p[2] = linear_to_srgb8(c[2]);
    p[3] = lround(c[3]*255);
}

void draw_triangle(Renderbuffer &color_attachment, const box2 &viewport, const Vert *verts) {
    auto area = [](const vec2 &p0, const vec2 &p1, const vec2 &p2) { return cross(p1 - p0, p2 - p0); };
    auto interpolate = [](const auto a[3], auto p, const vec3 &coord) { return coord.x*a[0].*p + coord.y*a[1].*p + coord.z*a[2].*p; };

    Varying perVertex[3];
    vec4 gl_Position[3];

    box2 aabb = { viewport.hi, viewport.lo };
    for(int i = 0; i < 3; ++i) {
        vertex_shader(verts[i], gl_Position[i], perVertex[i]);

        // convert to normalized device coordinates
        gl_Position[i].w = 1/gl_Position[i].w;
        gl_Position[i].xyz *= gl_Position[i].w;

        // convert to window coordinates
        gl_Position[i].xy = mix(viewport.lo, viewport.hi, 0.5f*(gl_Position[i].xy + 1.0f));
        aabb = join(aabb, gl_Position[i].xy);
    }

    const float denom = 1/area(gl_Position[0].xy, gl_Position[1].xy, gl_Position[2].xy);

    // loop over all pixels in the rectangle bounding the triangle
    const ibox2 iaabb = lround(aabb);
    for(int y = iaabb.lo.y; y < iaabb.hi.y; ++y)
    for(int x = iaabb.lo.x; x < iaabb.hi.x; ++x)
    {
        vec4 gl_FragCoord;
        gl_FragCoord.xy = vec2(x, y) + 0.5f;

        // fragment barycentric coordinates in window coordinates
        const vec3 barycentric = denom*vec3(
            area(gl_FragCoord.xy, gl_Position[1].xy, gl_Position[2].xy),
            area(gl_Position[0].xy, gl_FragCoord.xy, gl_Position[2].xy),
            area(gl_Position[0].xy, gl_Position[1].xy, gl_FragCoord.xy)
        );

        // discard fragment outside the triangle. this doesn't handle edges correctly.
        if(barycentric.x < 0 || barycentric.y < 0 || barycentric.z < 0)
            continue;

        // interpolate inverse depth linearly
        gl_FragCoord.z = interpolate(gl_Position, &vec4::z, barycentric);
        gl_FragCoord.w = interpolate(gl_Position, &vec4::w, barycentric);

        // clip fragments to the near/far planes (as if by GL_ZERO_TO_ONE)
        if(gl_FragCoord.z < 0 || gl_FragCoord.z > 1)
            continue;

        // convert to perspective correct (clip-space) barycentric
        const vec3 perspective = 1/gl_FragCoord.w*barycentric*vec3(gl_Position[0].w, gl_Position[1].w, gl_Position[2].w);

        // interpolate attributes
        Varying varying = {
            interpolate(perVertex, &Varying::texcoord, perspective),
            interpolate(perVertex, &Varying::color, perspective),
        };

        vec4 color;
        fragment_shader(gl_FragCoord, varying, color);
        rop(color_attachment, x, y, color);
    }
}

int main(int argc, char *argv[]) {
    Renderbuffer buffer = { 512, 512, 512*4 };
    buffer.data = calloc(buffer.ys, buffer.h);

    // VAO interleaved attributes buffer
    Vert verts[] = {
        { { -1, -1, -2, 1 }, { 0, 0, 0, 1 }, { 0, 0, 1, 1 } },
        { { 1, -1, -1, 1 }, { 10, 0, 0, 1 }, { 1, 0, 0, 1 } },
        { { 0, 1, -1, 1 }, { 0, 10, 0, 1 }, { 0, 1, 0, 1 } },
    };

    box2 viewport = { 0, 0, buffer.w, buffer.h };
    draw_triangle(buffer, viewport, verts);

    stbi_write_png("out.png", buffer.w, buffer.h, 4, buffer.data, buffer.ys);
}
*/

class BaseRendererCPU
{
    BaseRendererCPU()
    {
    }

private:
    void vertex_shader(const Vec4 &in, const Mat4 &tm, Vec4 &gl_Position, varyintType &OUT)
    {
        Vec4 tm_in = tm * in;
        // OUT.texcoord = in.texcoord;
        gl_Position = Vec4(tm_in(0), tm_in(1), -2 * tm_in(2) - 2 * tm_in(3), -tm_in(3));
    }

    template <typename varyingType, typename outType>
    void fragment_shader(const varyingType &IN, Vec4 &gl_FragCoord, outType &OUT)
    {
        OUT = IN.color;
        Vec2 wrapped = IN.texcoord.xy - floor(IN.texcoord.xy);
        bool brighter = (wrapped[0] < 0.5) != (wrapped[1] < 0.5);
        if (!brighter)
            OUT.rgb *= 0.5f;
    }

    // render output unit/render operations pipeline
    template <typename bufferType>
    void rop(bufferType &buf, int x, int y, const vec4 &c)
    {
        uint8_t *p = (uint8_t *)buf.data + buf.ys * (buf.h - y - 1) + 4 * x;
        p[0] = linear_to_srgb8(c[0]);
        p[1] = linear_to_srgb8(c[1]);
        p[2] = linear_to_srgb8(c[2]);
        p[3] = lround(c[3] * 255);
    }

    template <typename bufferType>
    void draw_triangle(const Vec4 *verts, const Vec2 *texcorrds, const Mat4 &tm, const box2 &viewport, bufferType &color_attachment)
    {
        auto area = [](const Vec2 &p0, const Vec2 &p1, const Vec2 &p2)
        { return cross(p1 - p0, p2 - p0); };
        auto interpolate = [](const auto a[3], auto p, const Vec3 &coord)
        { return coord.x * a[0].*p + coord.y * a[1].*p + coord.z * a[2].*p; };

        Varying perVertex[3];
        Vec4 gl_Position[3];

        box2 aabb = {viewport.hi, viewport.lo};
        for (int i = 0; i < 3; ++i)
        {
            vertex_shader(verts[i], tm, gl_Position[i], perVertex[i]);

            // convert to normalized device coordinates
            gl_Position[i].w = 1 / gl_Position[i].w;
            gl_Position[i].xyz *= gl_Position[i].w;

            // convert to window coordinates
            gl_Position[i].xy = mix(viewport.lo, viewport.hi, 0.5f * (gl_Position[i].xy + 1.0f));
            aabb = join(aabb, gl_Position[i].xy);
        }

        const float denom = 1 / area(gl_Position[0].xy, gl_Position[1].xy, gl_Position[2].xy);

        // loop over all pixels in the rectangle bounding the triangle
        const ibox2 iaabb = lround(aabb);
        for (int y = iaabb.lo.y; y < iaabb.hi.y; ++y)
            for (int x = iaabb.lo.x; x < iaabb.hi.x; ++x)
            {
                Vec4 gl_FragCoord;
                gl_FragCoord(0) = x + 0.5f;
                gl_FragCoord(1) = y + 0.5f;

                // fragment barycentric coordinates in window coordinates
                const Vec3 barycentric = denom * Vec3(
                                                     area(gl_FragCoord.xy, gl_Position[1].xy, gl_Position[2].xy),
                                                     area(gl_Position[0].xy, gl_FragCoord.xy, gl_Position[2].xy),
                                                     area(gl_Position[0].xy, gl_Position[1].xy, gl_FragCoord.xy));

                // discard fragment outside the triangle. this doesn't handle edges correctly.
                if (barycentric.x < 0 || barycentric.y < 0 || barycentric.z < 0)
                    continue;

                // interpolate inverse depth linearly
                gl_FragCoord.z = interpolate(gl_Position, &Vec4::z, barycentric);
                gl_FragCoord.w = interpolate(gl_Position, &Vec4::w, barycentric);

                // clip fragments to the near/far planes (as if by GL_ZERO_TO_ONE)
                if (gl_FragCoord.z < 0 || gl_FragCoord.z > 1)
                    continue;

                // convert to perspective correct (clip-space) barycentric
                const Vec3 perspective = 1 / gl_FragCoord.w * barycentric * Vec3(gl_Position[0].w, gl_Position[1].w, gl_Position[2].w);

                // interpolate attributes
                Varying varying = {
                    interpolate(perVertex, &Varying::texcoord, perspective),
                    interpolate(perVertex, &Varying::color, perspective),
                };

                Vec4 color;
                fragment_shader(gl_FragCoord, varying, color);
                rop(color_attachment, x, y, color);
            }
    }
}

class DepthRendererCPU : public BaseRendererCPU
{
    DepthRendererCPU() : BaseRendererCPU()
    {
    }

    void Render(MeshGL &mesh, SE3 pose, CameraType cam, TextureGL<float> &buffer, int lvl)
    {
        Mat4 view_matrix = cam.GetProjectiveMatrix(0.1f, 10.0f) * pose.matrix();

        for (int i = 0; i < mesh.tri_size; i += 3)
        {
            Vec4 p[3];
            Vec2 t[3];
            p[0](0) = mesh.pos_buffer_[mesh.ebo_buffer_[i] * 3];
            p[0](1) = mesh.pos_buffer_[mesh.ebo_buffer_[i] * 3 + 1];
            p[0](2) = mesh.pos_buffer_[mesh.ebo_buffer_[i] * 3 + 2];
            p[0](3) = 1.0f;

            t[0](0) = mesh.pos_buffer_[mesh.ebo_buffer_[i] * 2];
            t[0](1) = mesh.pos_buffer_[mesh.ebo_buffer_[i] * 2 + 1];

            p[1](0) = mesh.pos_buffer_[mesh.ebo_buffer_[i + 1] * 3];
            p[1](1) = mesh.pos_buffer_[mesh.ebo_buffer_[i + 1] * 3 + 1];
            p[1](2) = mesh.pos_buffer_[mesh.ebo_buffer_[i + 1] * 3 + 2];
            p[1](3) = 1.0f;

            t[1](0) = mesh.pos_buffer_[mesh.ebo_buffer_[i + 1] * 2];
            t[1](1) = mesh.pos_buffer_[mesh.ebo_buffer_[i + 1] * 2 + 1];

            p[2](0) = mesh.pos_buffer_[mesh.ebo_buffer_[i + 2] * 3];
            p[2](1) = mesh.pos_buffer_[mesh.ebo_buffer_[i + 2] * 3 + 1];
            p[2](2) = mesh.pos_buffer_[mesh.ebo_buffer_[i + 2] * 3 + 2];
            p[2](3) = 1.0f;

            t[2](0) = mesh.pos_buffer_[mesh.ebo_buffer_[i + 2] * 2];
            t[2](1) = mesh.pos_buffer_[mesh.ebo_buffer_[i + 2] * 2 + 1];

            draw_triangle(p, t, view_matrix, view_port, buffer)
        }
    }

private:
    void vertex_shader(const Vec4 &in, Mat4 tm, Vec4 &gl_Position, varyintType &OUT)
    {
        Vec4 tm_in = tm * in;
        // OUT.texcoord = in.texcoord;
        gl_Position = Vec4(tm_in(0), tm_in(1), -2 * tm_in(2) - 2 * tm_in(3), -tm_in(3));
    }

    template <typename varyingType, typename outType>
    void fragment_shader(Vec4 &gl_FragCoord, const varyingType &IN, outType &OUT)
    {
        OUT = IN.color;
        Vec2 wrapped = IN.texcoord.xy - floor(IN.texcoord.xy);
        bool brighter = (wrapped[0] < 0.5) != (wrapped[1] < 0.5);
        if (!brighter)
            OUT.rgb *= 0.5f;
    }
}
