#version 450
// Straight RGBA costume samples with Scratch graphical effects.
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 0, binding = 0) uniform sampler2D u_texture;
layout(push_constant) uniform Push {
    vec4 effects; // x=opacity, y=brightness, z=color, w=mosaic
    vec4 distort; // x=pixelate, y=whirl, z=fisheye, w=0
    vec4 texInfo; // x=texWidth, y=texHeight, z=0, w=0
} pc;

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main()
{
    vec2 uv = v_uv;
    const vec2 kCenter = vec2(0.5, 0.5);

    if (abs(pc.effects.w) > 0.001) {
        float tiles = clamp(round((abs(pc.effects.w) + 10.0) / 10.0 + 1e-3), 1.0, 512.0);
        uv = fract(tiles * uv);
    }

    if (abs(pc.distort.x) > 0.001) {
        float pixelateAmount = abs(pc.distort.x) / 10.0;
        vec2 pixelTexelSize = pc.texInfo.xy / max(pixelateAmount, 1e-3);
        uv = (floor(uv * pixelTexelSize) + kCenter) / pixelTexelSize;
    }

    vec2 centered = uv - kCenter;

    if (abs(pc.distort.y) > 0.001) {
        float whirlRad = pc.distort.y * 3.14159265 / 180.0;
        float dist = length(centered);
        float whirlFactor = max(1.0 - (dist / 0.5), 0.0);
        float whirlActual = whirlRad * whirlFactor * whirlFactor;
        float s = sin(whirlActual);
        float c = cos(whirlActual);
        centered = vec2(c * centered.x - s * centered.y, s * centered.x + c * centered.y);
    }

    if (abs(pc.distort.z) > 0.001) {
        float fisheyeAmount = max(0.0, (pc.distort.z + 100.0) / 100.0);
        vec2 v = centered / 0.5;
        float len = length(v);
        if (len > 0.0001) {
            float r = pow(min(len, 1.0), fisheyeAmount) * max(1.0, len);
            centered = r * (v / len) * 0.5;
        }
    }

    uv = centered + kCenter;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        discard;
    }

    vec4 sample_color = texture(u_texture, uv);
    vec3 rgb = sample_color.rgb;
    if (abs(pc.effects.z) > 0.001) {
        vec3 hsv = rgb2hsv(rgb);
        const float minLightness = 0.11 / 2.0;
        const float minSaturation = 0.09;
        if (hsv.z < minLightness) hsv = vec3(0.0, 1.0, minLightness);
        else if (hsv.y < minSaturation) hsv = vec3(0.0, minSaturation, hsv.z);
        hsv.x = fract(hsv.x + pc.effects.z / 200.0);
        rgb = hsv2rgb(hsv);
    }
    o_color = vec4(clamp(rgb + vec3(pc.effects.y), 0.0, 1.0), sample_color.a * pc.effects.x);
}
