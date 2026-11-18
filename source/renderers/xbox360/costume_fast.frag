#version 450
// Lightweight straight-RGBA pixel shader for ordinary sprites, ghost, brightness, text, and pen-layer quads.
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 0, binding = 0) uniform sampler2D u_texture;
layout(push_constant) uniform Push {
    vec4 effects; // x=opacity, y=brightness, z=0, w=0
} pc;

void main()
{
    vec4 sample_color = texture(u_texture, v_uv);
    o_color = vec4(clamp(sample_color.rgb + vec3(pc.effects.y), 0.0, 1.0), sample_color.a * pc.effects.x);
}
