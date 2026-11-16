#pragma once

#define SE_GX2_GLSL_VERSION "#version 410 core\n"

namespace GX2Shaders {

static const char *kSpriteVert =
    SE_GX2_GLSL_VERSION
    R"glsl(
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;

out vec2 v_uv;

uniform mat4 u_model;
uniform mat4 u_projection;

void main() {
    gl_Position = u_projection * u_model * vec4(a_pos, 0.0, 1.0);
    v_uv = a_uv;
}
)glsl";

static const char *kSpriteFrag =
    SE_GX2_GLSL_VERSION
    R"glsl(
in  vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_tex;
uniform vec4 u_params0;
uniform vec4 u_params1;
uniform vec4 u_params2;

const float epsilon = 1e-3;
const vec2 kCenter = vec2(0.5, 0.5);

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    float u_opacity = u_params0.x;
    float u_brightness = u_params0.y;
    float u_color = u_params0.z;
    float u_fisheye = u_params0.w;
    float u_whirl = u_params1.x;
    float u_pixelate = u_params1.y;
    float u_mosaic = u_params1.z;
    vec2 u_tex_size = vec2(u_params1.w, u_params2.x);

    vec2 uv = v_uv;

    if (abs(u_mosaic) > 0.001) {
        float tiles = clamp(round((abs(u_mosaic) + 10.0) / 10.0 + epsilon), 1.0, 512.0);
        uv = fract(tiles * uv);
    }

    if (abs(u_pixelate) > 0.001) {
        float pixelateAmount = abs(u_pixelate) / 10.0;
        vec2 pixelTexelSize = u_tex_size / max(pixelateAmount, epsilon);
        uv = (floor(uv * pixelTexelSize) + kCenter) / pixelTexelSize;
    }

    vec2 centered = uv - kCenter;

    if (abs(u_whirl) > 0.001) {
        float whirlRad = u_whirl * 3.14159265 / 180.0;
        float kRadius = 0.5;
        float dist = length(centered);
        float whirlFactor = max(1.0 - (dist / kRadius), 0.0);
        float whirlActual = whirlRad * whirlFactor * whirlFactor;
        float s = sin(whirlActual);
        float cosA = cos(whirlActual);
        centered = vec2(cosA * centered.x - s * centered.y,
                        s * centered.x + cosA * centered.y);
    }

    if (abs(u_fisheye) > 0.001) {
        float fisheyeAmount = max(0.0, (u_fisheye + 100.0) / 100.0);
        vec2 vec = centered / 0.5;
        float vecLength = length(vec);
        if (vecLength > 0.0001) {
            float r = pow(min(vecLength, 1.0), fisheyeAmount) * max(1.0, vecLength);
            vec2 unit = vec / vecLength;
            centered = r * unit * 0.5;
        }
    }

    uv = centered + kCenter;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        discard;
    }

    vec4 color = texture(u_tex, uv);
    if (color.a < 0.001) discard;

    if (abs(u_color) > 0.001) {
        vec3 hsv = rgb2hsv(color.rgb);

        const float minLightness = 0.11 / 2.0;
        const float minSaturation = 0.09;
        if (hsv.z < minLightness) hsv = vec3(0.0, 1.0, minLightness);
        else if (hsv.y < minSaturation) hsv = vec3(0.0, minSaturation, hsv.z);

        hsv.x = fract(hsv.x + u_color / 200.0);
        color.rgb = hsv2rgb(hsv);
    }

    if (abs(u_brightness) > 0.001) {
        float brightnessAmount = clamp(u_brightness, -100.0, 100.0) / 100.0;
        color.rgb = clamp(color.rgb + vec3(brightnessAmount), 0.0, 1.0);
    }

    color.a *= u_opacity;

    frag_color = color;
}
)glsl";

static const char *kSolidVert =
    SE_GX2_GLSL_VERSION
    R"glsl(
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec4 a_color;

out vec4 v_color;

uniform mat4 u_projection;

void main() {
    gl_Position = u_projection * vec4(a_pos, 0.0, 1.0);
    v_color = a_color;
}
)glsl";

static const char *kSolidFrag =
    SE_GX2_GLSL_VERSION
    R"glsl(
in vec4 v_color;
out vec4 frag_color;

void main() {
    frag_color = v_color;
}
)glsl";

static const char *kTextVert =
    SE_GX2_GLSL_VERSION
    R"glsl(
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
uniform mat4 u_projection;
void main() {
    gl_Position = u_projection * vec4(a_pos, 0.0, 1.0);
    v_uv = a_uv;
}
)glsl";

static const char *kTextFrag =
    SE_GX2_GLSL_VERSION
    R"glsl(
in  vec2 v_uv;
out vec4 frag_color;
uniform sampler2D u_tex;
uniform vec4      u_color;
void main() {
    float alpha = texture(u_tex, v_uv).r;
    frag_color = vec4(u_color.rgb, u_color.a * alpha);
}
)glsl";

} // namespace GX2Shaders
