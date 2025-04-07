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

    /*
    struct Varying
    {
        vec4 texcoord, color;
    };
    */

    void render()
    {
    }

private:
    template <typename vertexType, typename varyintType>
    void vertex_shader(const vertexType &in, vec4f &gl_Position, varyintType &OUT)
    {
        // OUT.texcoord = in.texcoord;
        gl_Position = vec4(in.position.x, in.position.y, -2 * in.position.z - 2 * in.position.w, -in.position.z);
    }

    template <typename varyingType, typename outType>
    void fragment_shader(vec4 &gl_FragCoord, const varyingType &IN, outType &OUT)
    {
        OUT = IN.color;
        vec2 wrapped = IN.texcoord.xy - floor(IN.texcoord.xy);
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

    template <typename vertexType, typename bufferType>
    void draw_triangle(bufferType &color_attachment, const box2 &viewport, const vertexType *verts)
    {
        auto area = [](const vec2 &p0, const vec2f &p1, const vec2 &p2)
        { return cross(p1 - p0, p2 - p0); };
        auto interpolate = [](const auto a[3], auto p, const vec3 &coord)
        { return coord.x * a[0].*p + coord.y * a[1].*p + coord.z * a[2].*p; };

        Varying perVertex[3];
        vec4f gl_Position[3];

        box2 aabb = {viewport.hi, viewport.lo};
        for (int i = 0; i < 3; ++i)
        {
            vertex_shader(verts[i], gl_Position[i], perVertex[i]);

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
                vec4f gl_FragCoord;
                gl_FragCoord.xy = vec2f(x, y) + 0.5f;

                // fragment barycentric coordinates in window coordinates
                const vec3f barycentric = denom * vec3f(
                                                     area(gl_FragCoord.xy, gl_Position[1].xy, gl_Position[2].xy),
                                                     area(gl_Position[0].xy, gl_FragCoord.xy, gl_Position[2].xy),
                                                     area(gl_Position[0].xy, gl_Position[1].xy, gl_FragCoord.xy));

                // discard fragment outside the triangle. this doesn't handle edges correctly.
                if (barycentric.x < 0 || barycentric.y < 0 || barycentric.z < 0)
                    continue;

                // interpolate inverse depth linearly
                gl_FragCoord.z = interpolate(gl_Position, &vec4f::z, barycentric);
                gl_FragCoord.w = interpolate(gl_Position, &vec4f::w, barycentric);

                // clip fragments to the near/far planes (as if by GL_ZERO_TO_ONE)
                if (gl_FragCoord.z < 0 || gl_FragCoord.z > 1)
                    continue;

                // convert to perspective correct (clip-space) barycentric
                const vec3f perspective = 1 / gl_FragCoord.w * barycentric * vec3(gl_Position[0].w, gl_Position[1].w, gl_Position[2].w);

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

}
