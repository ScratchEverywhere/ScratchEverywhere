#include "render.hpp"
#include "speech_manager_gl_core.hpp"
#include <image_gl_core.hpp>
#include <log.hpp>
#include <window.hpp>
#if defined(WINDOWING_GLFW)
#include <windowing/glfw/window.hpp>
#elif defined(WINDOWING_SDL1)
#include <windowing/sdl1/window.hpp>
#elif defined(WINDOWING_SDL2)
#include <windowing/sdl2/window.hpp>
#elif defined(WINDOWING_SDL3)
#include <windowing/sdl3/window.hpp>
#elif defined(WINDOWING_LIBRETRO)
#include <windowing/libretro/window.hpp>
#else
#error "No windowing backend defined"
#endif
#include <algorithm>
#include <audio.hpp>
#include <chrono>
#include <cmath>
#include <color.hpp>
#include <cstdlib>
#include <downloader.hpp>
#include <image.hpp>
#include <math.hpp>
#include <render.hpp>
#include <runtime.hpp>
#include <sprite.hpp>
#include <string>
#include <unordered_map>
#include <unzip.hpp>
#include <vector>

#ifdef LIBRETRO
#include <libretro.h>

extern struct retro_hw_render_callback hw_render;
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

WindowSE *globalWindow = nullptr;
SpeechManagerGLCore *speechManager = nullptr;

static GLuint penFBO = 0;
static GLuint penTexture = 0;
static int penWidth = 0;
static int penHeight = 0;

GLuint spriteProgram = 0;

static GLuint solidProgram = 0;

GLuint quadVAO = 0;
static GLuint quadVBO = 0;

static GLuint compileShader(GLenum type, const char *src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Log::logError(std::string("[GL Core] Shader compile error: ") + log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint linkProgram(const char *vertSrc, const char *fragSrc) {
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!vert || !frag) {
        glDeleteShader(vert);
        glDeleteShader(frag);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        Log::logError(std::string("[GL Core] Program link error: ") + log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static const char *kSpriteVert = R"glsl(
#version 410 core

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

static const char *kSpriteFrag = R"glsl(
#version 410 core

in  vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_tex;
uniform float u_opacity;
uniform float u_brightness;
uniform float u_color;
uniform float u_fisheye;
uniform float u_whirl;
uniform float u_pixelate;
uniform float u_mosaic;
uniform vec2  u_tex_size;

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

static const char *kSolidVert = R"glsl(
#version 410 core

layout(location = 0) in vec2 a_pos;

uniform mat4 u_projection;

void main() {
    gl_Position = u_projection * vec4(a_pos, 0.0, 1.0);
}
)glsl";

static const char *kSolidFrag = R"glsl(
#version 410 core

out vec4 frag_color;
uniform vec4 u_color;

void main() {
    frag_color = u_color;
}
)glsl";

static void buildOrtho(float out[16], float l, float r, float b, float t) {
    for (int i = 0; i < 16; ++i)
        out[i] = 0.0f;
    out[0] = 2.0f / (r - l);
    out[5] = 2.0f / (t - b);
    out[10] = -1.0f;
    out[12] = -(r + l) / (r - l);
    out[13] = -(t + b) / (t - b);
    out[15] = 1.0f;
}

static void setupQuadGeometry() {
    const float verts[] = {
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
    };
    static const GLuint indices[] = {0, 1, 2, 2, 3, 0};

    GLuint ebo = 0;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glGenBuffers(1, &ebo);

    glBindVertexArray(quadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

    glBindVertexArray(0);
}

static GLuint getMainFBO() {
#ifdef LIBRETRO
    return (GLuint)hw_render.get_current_framebuffer();
#else
    return 0;
#endif
}

static bool createPenFBO() {
    if (Scratch::hqpen) {
        if (Scratch::projectWidth / static_cast<double>(Render::getWidth()) <
            Scratch::projectHeight / static_cast<double>(Render::getHeight())) {
            penWidth = Scratch::projectWidth * (Render::getHeight() / static_cast<double>(Scratch::projectHeight));
            penHeight = Render::getHeight();
        } else {
            penWidth = Render::getWidth();
            penHeight = Scratch::projectHeight * (Render::getWidth() / static_cast<double>(Scratch::projectWidth));
        }
    } else {
        penWidth = Scratch::projectWidth;
        penHeight = Scratch::projectHeight;
    }

    glGenTextures(1, &penTexture);
    glBindTexture(GL_TEXTURE_2D, penTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, penWidth, penHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenFramebuffers(1, &penFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, penFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, penTexture, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, getMainFBO());

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        Log::logError("[GL Core] Pen FBO incomplete");
        glDeleteFramebuffers(1, &penFBO);
        glDeleteTextures(1, &penTexture);
        penFBO = penTexture = 0;
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, penFBO);
    glViewport(0, 0, penWidth, penHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, getMainFBO());

    return true;
}

static void destroyPenFBO() {
    if (penFBO) {
        glDeleteFramebuffers(1, &penFBO);
        penFBO = 0;
    }
    if (penTexture) {
        glDeleteTextures(1, &penTexture);
        penTexture = 0;
    }
    penWidth = penHeight = 0;
}

static void drawSolidRect(float x, float y, float w, float h,
                          float r, float g, float b, float a,
                          const float proj[16]) {
    glUseProgram(solidProgram);
    glUniformMatrix4fv(glGetUniformLocation(solidProgram, "u_projection"), 1, GL_FALSE, proj);
    glUniform4f(glGetUniformLocation(solidProgram, "u_color"), r, g, b, a);

    float verts[] = {
        x,
        y,
        x + w,
        y,
        x + w,
        y + h,
        x,
        y + h,
    };
    static const GLuint idx[] = {0, 1, 2, 2, 3, 0};

    GLuint vao = 0, vbo = 0, ebo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
}

static void drawSolidCircle(float cx, float cy, float radius,
                            float r, float g, float b, float a,
                            const float proj[16], int segments = 24) {
    glUseProgram(solidProgram);
    glUniformMatrix4fv(glGetUniformLocation(solidProgram, "u_projection"), 1, GL_FALSE, proj);
    glUniform4f(glGetUniformLocation(solidProgram, "u_color"), r, g, b, a);

    std::vector<float> verts;
    verts.reserve((segments + 2) * 2);
    verts.push_back(cx);
    verts.push_back(cy);
    for (int i = 0; i <= segments; ++i) {
        float angle = i * 2.0f * (float)M_PI / segments;
        verts.push_back(cx + std::cos(angle) * radius);
        verts.push_back(cy + std::sin(angle) * radius);
    }

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    glDrawArrays(GL_TRIANGLE_FAN, 0, (GLsizei)(verts.size() / 2));

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

static void drawSolidQuad(float x0, float y0, float x1, float y1,
                          float x2, float y2, float x3, float y3,
                          float r, float g, float b, float a,
                          const float proj[16]) {
    glUseProgram(solidProgram);
    glUniformMatrix4fv(glGetUniformLocation(solidProgram, "u_projection"), 1, GL_FALSE, proj);
    glUniform4f(glGetUniformLocation(solidProgram, "u_color"), r, g, b, a);

    float verts[] = {x0, y0, x1, y1, x2, y2, x3, y3};
    static const GLuint idx[] = {0, 1, 2, 2, 3, 0};

    GLuint vao = 0, vbo = 0, ebo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
}

bool Render::Init() {
#if defined(WINDOWING_GLFW)
    globalWindow = new WindowGLFW();
#elif defined(WINDOWING_SDL1)
    globalWindow = new WindowSDL1();
#elif defined(WINDOWING_SDL2)
    globalWindow = new WindowSDL2();
#elif defined(WINDOWING_SDL3)
    globalWindow = new WindowSDL3();
#elif defined(WINDOWING_LIBRETRO)
    globalWindow = new WindowLibretro();
#else
#error "No windowing backend defined"
#endif

    if (!globalWindow->init(540, 405, "Scratch Everywhere!")) {
        delete globalWindow;
        globalWindow = nullptr;
        return false;
    }

#if defined(WINDOWING_LIBRETRO)
    if (!gladLoadGL((GLADloadfunc)hw_render.get_proc_address)) {
        Log::logError("[GL Core] Failed to initialize GLAD via Libretro proc address");
        globalWindow->cleanup();
        delete globalWindow;
        globalWindow = nullptr;
        return false;
    }
#else
    if (!gladLoaderLoadGL()) {
        Log::logError("[GL Core] Failed to initialize GLAD");
        globalWindow->cleanup();
        delete globalWindow;
        globalWindow = nullptr;
        return false;
    }
#endif

    const char *renderer = (const char *)glGetString(GL_RENDERER);
    const char *version = (const char *)glGetString(GL_VERSION);
    Log::log(std::string("[GL Core] Renderer: ") + (renderer ? renderer : "?"));
    Log::log(std::string("[GL Core] Version:  ") + (version ? version : "?"));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    spriteProgram = linkProgram(kSpriteVert, kSpriteFrag);
    solidProgram = linkProgram(kSolidVert, kSolidFrag);

    if (!spriteProgram || !solidProgram) {
        Log::logError("[GL Core] Failed to compile/link shaders");
        globalWindow->cleanup();
        delete globalWindow;
        globalWindow = nullptr;
        return false;
    }

    setupQuadGeometry();
    setRenderScale();

    debugMode = true;
    return true;
}

void Render::deInit() {
    destroyPenFBO();

    if (spriteProgram) {
        glDeleteProgram(spriteProgram);
        spriteProgram = 0;
    }
    if (solidProgram) {
        glDeleteProgram(solidProgram);
        solidProgram = 0;
    }
    if (quadVAO) {
        glDeleteVertexArrays(1, &quadVAO);
        quadVAO = 0;
    }
    if (quadVBO) {
        glDeleteBuffers(1, &quadVBO);
        quadVBO = 0;
    }

    SoundPlayer::deinit();
    TextObject::cleanupText();

    if (speechManager) {
        delete speechManager;
        speechManager = nullptr;
    }

    if (globalWindow) {
        globalWindow->cleanup();
        delete globalWindow;
        globalWindow = nullptr;
    }
}

void *Render::getRenderer() { return nullptr; }

bool Render::createSpeechManager() {
    if (speechManager == nullptr) speechManager = new SpeechManagerGLCore();
    return speechManager != nullptr;
}

void Render::destroySpeechManager() {
    delete speechManager;
    speechManager = nullptr;
}

SpeechManager *Render::getSpeechManager() { return speechManager; }

int Render::getWidth() { return globalWindow ? globalWindow->getWidth() : 540; }
int Render::getHeight() { return globalWindow ? globalWindow->getHeight() : 405; }

float Render::getPixelDensity() {
    return globalWindow ? globalWindow->getPixelDensity() : 1.0f;
}

bool Render::initPen() {
    if (penFBO != 0) return true;
    return createPenFBO();
}

void Render::penClear() {
    if (penFBO == getMainFBO()) return;
    glBindFramebuffer(GL_FRAMEBUFFER, penFBO);
    glViewport(0, 0, penWidth, penHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, getMainFBO());
}

static void penBegin() {
    glBindFramebuffer(GL_FRAMEBUFFER, penFBO);
    glViewport(0, 0, penWidth, penHeight);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void penEnd() {
    glBindFramebuffer(GL_FRAMEBUFFER, getMainFBO());
    glViewport(0, 0, Render::getWidth(), Render::getHeight());
}

void Render::penMoveFast(double x1, double y1, double x2, double y2, Sprite *sprite) {
    penMoveAccurate(x1, y1, x2, y2, sprite);
}

void Render::penDotFast(Sprite *sprite) {
    penDotAccurate(sprite);
}

void Render::penMoveAccurate(double x1, double y1, double x2, double y2, Sprite *sprite) {
    if (penFBO == 0) return;

    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0f - (float)sprite->penData.color.transparency) / 100.0f;
    float r = rgbColor.r / 255.0f, g = rgbColor.g / 255.0f, b = rgbColor.b / 255.0f;

    const double scale = penHeight / static_cast<double>(Scratch::projectHeight);
    float px1 = (float)(x1 * scale + penWidth / 2.0);
    float py1 = (float)(-y1 * scale + penHeight / 2.0);
    float px2 = (float)(x2 * scale + penWidth / 2.0);
    float py2 = (float)(-y2 * scale + penHeight / 2.0);
    float radius = (float)((sprite->penData.size / 2.0) * scale);

    float proj[16];
    buildOrtho(proj, 0.0f, (float)penWidth, (float)penHeight, 0.0f);

    penBegin();

    float dx = px2 - px1, dy = py2 - py1;
    float length = std::sqrt(dx * dx + dy * dy);
    if (length > 0.001f) {
        float nx = -dy / length * radius;
        float ny = dx / length * radius;
        drawSolidQuad(px1 + nx, py1 + ny,
                      px1 - nx, py1 - ny,
                      px2 - nx, py2 - ny,
                      px2 + nx, py2 + ny,
                      r, g, b, alpha, proj);
    }

    drawSolidCircle(px1, py1, radius, r, g, b, alpha, proj);
    drawSolidCircle(px2, py2, radius, r, g, b, alpha, proj);

    penEnd();
}

void Render::penDotAccurate(Sprite *sprite) {
    if (penFBO == 0) return;

    const ColorRGBA rgbColor = CSBT2RGBA(sprite->penData.color);
    float alpha = (100.0f - (float)sprite->penData.color.transparency) / 100.0f;
    float r = rgbColor.r / 255.0f, g = rgbColor.g / 255.0f, b = rgbColor.b / 255.0f;

    const double scale = penHeight / static_cast<double>(Scratch::projectHeight);
    float px = (float)(sprite->xPosition * scale + penWidth / 2.0);
    float py = (float)(-sprite->yPosition * scale + penHeight / 2.0);
    float radius = (float)((sprite->penData.size / 2.0) * scale);

    float proj[16];
    buildOrtho(proj, 0.0f, (float)penWidth, (float)penHeight, 0.0f);

    penBegin();
    drawSolidCircle(px, py, radius, r, g, b, alpha, proj);
    penEnd();
}

void Render::penStamp(Sprite *sprite) {
    if (penFBO == 0) return;

    const auto &imgFind = Scratch::costumeImages.find(sprite->costumes[sprite->currentCostume].fullName);
    if (imgFind == Scratch::costumeImages.end()) {
        Log::logWarning("Invalid Image for Stamp");
        return;
    }

    Image_GLCore *image = reinterpret_cast<Image_GLCore *>(imgFind->second.get());

    const bool isSVG = sprite->costumes[sprite->currentCostume].isSVG;
    Render::calculateRenderPosition(sprite, isSVG);

    auto cords = Scratch::screenToScratchCoords(sprite->renderInfo.renderX, sprite->renderInfo.renderY, getWidth(), getHeight());
    float penX = (float)(cords.first + Scratch::projectWidth / 2.0);
    float penY = (float)(-cords.second + Scratch::projectHeight / 2.0);

    const double scale = penHeight / static_cast<double>(Scratch::projectHeight);
    if (Scratch::hqpen) {
        penX *= (float)scale;
        penY *= (float)scale;
    }

    float renderScale = Scratch::hqpen ? sprite->renderInfo.renderScaleY : sprite->size / 100.0f;

    ImageRenderParams params;
    params.centered = true;
    params.x = penX;
    params.y = penY;
    params.scale = renderScale;
    params.rotation = sprite->renderInfo.renderRotation;
    params.flip = (sprite->rotationStyle == sprite->LEFT_RIGHT && sprite->rotation < 0);
    params.opacity = 1.0f - std::clamp(sprite->ghostEffect, 0.0f, 100.0f) * 0.01f;
    params.brightness = sprite->brightnessEffect;

    penBegin();
    image->render(params);
    penEnd();
}

void Render::beginFrame(int /*screen*/, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        glViewport(0, 0, getWidth(), getHeight());
        glClearColor(colorR / 255.0f, colorG / 255.0f, colorB / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        hasFrameBegan = true;
    }
}

void Render::endFrame(bool shouldFlush) {
    if (globalWindow) globalWindow->swapBuffers();
    hasFrameBegan = false;
}

void Render::drawBox(int w, int h, int x, int y,
                     uint8_t colorR, uint8_t colorG, uint8_t colorB, uint8_t colorA) {
    float proj[16];
    buildOrtho(proj, 0.0f, (float)getWidth(), (float)getHeight(), 0.0f);

    drawSolidRect((float)(x - w / 2), (float)(y - h / 2), (float)w, (float)h,
                  colorR / 255.0f, colorG / 255.0f, colorB / 255.0f, colorA / 255.0f,
                  proj);
}

void Render::renderPenLayer() {
    if (penTexture == 0) return;

    float projectAspect = (float)Scratch::projectWidth / Scratch::projectHeight;
    float windowAspect = (float)getWidth() / getHeight();

    float drawW, drawH, drawX, drawY;
    if (windowAspect > projectAspect) {
        drawH = (float)getHeight();
        drawW = drawH * projectAspect;
        drawX = (getWidth() - drawW) / 2.0f;
        drawY = 0;
    } else {
        drawW = (float)getWidth();
        drawH = drawW / projectAspect;
        drawX = 0;
        drawY = (getHeight() - drawH) / 2.0f;
    }

    float proj[16];
    buildOrtho(proj, 0.0f, (float)getWidth(), (float)getHeight(), 0.0f);

    float model[16] = {};
    model[0] = drawW;
    model[5] = -drawH;
    model[10] = 1.0f;
    model[12] = drawX;
    model[13] = drawY + drawH;
    model[15] = 1.0f;

    glUseProgram(spriteProgram);
    glUniformMatrix4fv(glGetUniformLocation(spriteProgram, "u_projection"), 1, GL_FALSE, proj);
    glUniformMatrix4fv(glGetUniformLocation(spriteProgram, "u_model"), 1, GL_FALSE, model);
    glUniform1i(glGetUniformLocation(spriteProgram, "u_tex"), 0);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_opacity"), 1.0f);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_brightness"), 0.0f);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_color"), 0.0f);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_fisheye"), 0.0f);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_whirl"), 0.0f);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_pixelate"), 0.0f);
    glUniform1f(glGetUniformLocation(spriteProgram, "u_mosaic"), 0.0f);
    glUniform2f(glGetUniformLocation(spriteProgram, "u_tex_size"), (float)penWidth, (float)penHeight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, penTexture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(quadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

static void drawBlackBars(int screenWidth, int screenHeight, const float proj[16]) {
    float screenAspect = (float)screenWidth / screenHeight;
    float projectAspect = (float)Scratch::projectWidth / Scratch::projectHeight;

    if (screenAspect > projectAspect) {
        float scale = (float)screenHeight / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

        drawSolidRect(0, 0, barWidth, (float)screenHeight, 0, 0, 0, 1, proj);
        drawSolidRect(screenWidth - barWidth, 0, barWidth, (float)screenHeight, 0, 0, 0, 1, proj);
    } else if (screenAspect < projectAspect) {
        float scale = (float)screenWidth / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (screenHeight - scaledProjectHeight) / 2.0f;

        drawSolidRect(0, 0, (float)screenWidth, barHeight, 0, 0, 0, 1, proj);
        drawSolidRect(0, screenHeight - barHeight, (float)screenWidth, barHeight, 0, 0, 0, 1, proj);
    }
}

void Render::renderSprites() {
    glViewport(0, 0, getWidth(), getHeight());
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float proj[16];
    buildOrtho(proj, 0.0f, (float)getWidth(), (float)getHeight(), 0.0f);

    for (auto it = Scratch::sprites.rbegin(); it != Scratch::sprites.rend(); ++it) {
        Sprite *currentSprite = *it;

        auto imgFind = Scratch::costumeImages.find(currentSprite->costumes[currentSprite->currentCostume].fullName);
        if (imgFind != Scratch::costumeImages.end()) {
            Image_GLCore *image = reinterpret_cast<Image_GLCore *>(imgFind->second.get());

            const bool isSVG = currentSprite->costumes[currentSprite->currentCostume].isSVG;
            calculateRenderPosition(currentSprite, isSVG);
            if (!currentSprite->visible) continue;

            ImageRenderParams params;
            params.centered = true;
            params.x = currentSprite->renderInfo.renderX;
            params.y = currentSprite->renderInfo.renderY;
            params.rotation = currentSprite->renderInfo.renderRotation;
            params.scale = currentSprite->renderInfo.renderScaleY;
            params.flip = (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT && currentSprite->rotation < 0);
            params.opacity = 1.0f - std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f) * 0.01f;
            params.brightness = currentSprite->brightnessEffect;
            params.colorEffect = currentSprite->colorEffect;
            params.fisheyeEffect = currentSprite->fisheyeEffect;
            params.whirlEffect = currentSprite->whirlEffect;
            params.pixelateEffect = currentSprite->pixelateEffect;
            params.mosaicEffect = currentSprite->mosaicEffect;

            image->render(params);
        }

        if (currentSprite->isStage) renderPenLayer();
    }

    if (speechManager) speechManager->render();

    drawBlackBars(getWidth(), getHeight(), proj);
    renderMonitors();

    endFrame(true);
}

bool Render::appShouldRun() {
    if (OS::toExit) return false;
    if (globalWindow) {
        globalWindow->pollEvents();

        static int lastW = 0, lastH = 0;
        int currentW = globalWindow->getWidth();
        int currentH = globalWindow->getHeight();

        if (lastW != currentW || lastH != currentH) {
            lastW = currentW;
            lastH = currentH;

            if (Scratch::hqpen) {
                destroyPenFBO();
                createPenFBO();
            }
        }

        return !globalWindow->shouldClose();
    }
    return false;
}
