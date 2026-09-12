// SPDX-License-Identifier: GPL-2.0-or-later
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 uv;
};

/// Fullscreen triangle, scaled by `fit` to letterbox the guest surface without
/// distorting its aspect ratio. No vertex buffer: the positions are derived from
/// the vertex id.
vertex VertexOut husk_vertex(uint vid [[vertex_id]],
                             constant float2 &fit [[buffer(0)]])
{
    // Two triangles covering [-1,1] in clip space.
    const float2 corners[4] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1) };
    const uint idx[6] = { 0, 1, 2, 2, 1, 3 };

    float2 p = corners[idx[vid]];
    VertexOut out;
    out.position = float4(p * fit, 0, 1);
    // Flip Y: the guest framebuffer's first row is the top of the screen, but
    // clip-space +Y is up.
    out.uv = float2((p.x + 1.0) * 0.5, 1.0 - (p.y + 1.0) * 0.5);
    return out;
}

fragment float4 husk_fragment(VertexOut in [[stage_in]],
                              texture2d<float> guest [[texture(0)]])
{
    constexpr sampler s(filter::linear, address::clamp_to_edge);
    return guest.sample(s, in.uv);
}
