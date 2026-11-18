#!/usr/bin/env python3
"""Host disassembly and wavefront simulation checks for Xbox 360 costume shader."""
from __future__ import annotations

import colorsys
import math
import pathlib
import re
import struct
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = ROOT / "include/renderers/xbox360/costume_shaders_xbox360.hpp"
TOOLCHAIN = pathlib.Path("/home/mikedev/xsdk/xenos-toolchain")
sys.path.insert(0, str(TOOLCHAIN))

from xdis.simulate import run_group  # noqa: E402


def load_shader(prefix: str = "costume_frag"):
    text = HEADER.read_text(encoding="utf-8")
    m = re.search(rf"{prefix}_container\[\]\s*=\s*\{{(.*?)\}};", text, re.S)
    assert m, f"Missing {prefix}_container"
    words = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", m.group(1))]
    blob = struct.pack(f">{len(words)}I", *words)
    literals: dict[int, list[float]] = {}
    lm_block = re.search(rf"{prefix}_literals\[\]\s*=\s*\{{(.*?)\}};", text, re.S)
    if lm_block:
        for lm in re.finditer(r"\{\s*(\d+),\s*\{\s*([^}]+)\}\s*\}", lm_block.group(1)):
            reg = int(lm.group(1))
            literals[reg] = [float(x.rstrip("f ")) for x in lm.group(2).split(",")]
    return blob, literals


def inspect_disasm(blob: bytes) -> tuple[str, int, int]:
    with tempfile.NamedTemporaryFile(suffix=".bin") as tf:
        tf.write(blob)
        tf.flush()
        dis = subprocess.run(
            ["python3", str(TOOLCHAIN / "xdis/xdis.py"), tf.name],
            capture_output=True,
            text=True,
            check=True,
        ).stdout
    slots_m = re.search(r"(\d+) instr slots", dis)
    pc_m = re.search(r"program_control\s+0x([0-9a-fA-F]+)", dis)
    assert slots_m and pc_m, dis
    slots = int(slots_m.group(1))
    pc = int(pc_m.group(1), 16)
    gpr_count = ((pc >> 8) & 0x3F) + 1
    return dis, slots, gpr_count


def ref_frag(
    u: float,
    v: float,
    effects: tuple[float, float, float, float] = (1.0, 0.0, 0.0, 0.0),
    distort: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 0.0),
    texinfo: tuple[float, float, float, float] = (64.0, 64.0, 0.0, 0.0),
):
    opacity, brightness, color_fx, mosaic = effects
    pixelate, whirl, fisheye, _ = distort
    tw, th = texinfo[0], texinfo[1]
    if abs(mosaic) > 0.001:
        tiles = min(max(float(int(round((abs(mosaic) + 10.0) / 10.0 + 1e-3))), 1.0), 512.0)
        u = (u * tiles) % 1.0
        v = (v * tiles) % 1.0
    if abs(pixelate) > 0.001:
        amt = max(abs(pixelate) / 10.0, 1e-3)
        pxw, pxh = tw / amt, th / amt
        u = (math.floor(u * pxw) + 0.5) / pxw
        v = (math.floor(v * pxh) + 0.5) / pxh
    cx, cy = u - 0.5, v - 0.5
    if abs(whirl) > 0.001:
        rad = whirl * 3.14159265 / 180.0
        d = math.hypot(cx, cy)
        f = max(1.0 - d / 0.5, 0.0)
        wa = rad * f * f
        s, c = math.sin(wa), math.cos(wa)
        cx, cy = c * cx - s * cy, s * cx + c * cy
    if abs(fisheye) > 0.001:
        amt = max(0.0, (fisheye + 100.0) / 100.0)
        vx, vy = cx / 0.5, cy / 0.5
        ln = math.hypot(vx, vy)
        if ln > 0.0001:
            r = (min(ln, 1.0) ** amt) * max(1.0, ln)
            cx, cy = r * (vx / ln) * 0.5, r * (vy / ln) * 0.5
    nu, nv = cx + 0.5, cy + 0.5
    killed = nu < 0.0 or nu > 1.0 or nv < 0.0 or nv > 1.0
    r, g, b, a = nu, nv, 0.4, 0.8
    if abs(color_fx) > 0.001:
        h, s, val = colorsys.rgb_to_hsv(r, g, b)
        min_l, min_s = 0.11 / 2.0, 0.09
        if val < min_l:
            h, s, val = 0.0, 1.0, min_l
        elif s < min_s:
            h, s, val = 0.0, min_s, val
        h = (h + color_fx / 200.0) % 1.0
        r, g, b = colorsys.hsv_to_rgb(h, s, val)
    ro = min(max(r + brightness, 0.0), 1.0)
    go = min(max(g + brightness, 0.0), 1.0)
    bo = min(max(b + brightness, 0.0), 1.0)
    return killed, (ro, go, bo, a * opacity)


def run_host_quad_selection_tests() -> None:
    """Compile and execute production TexturedQuadXbox360 shader-selection logic on host."""
    quad_src = (ROOT / "source/renderers/xbox360/textured_quad_xbox360.cpp").read_text(encoding="utf-8")
    # Remove only PowerPC barrier intrinsic when compiling the X86 host test binary.
    quad_src = re.sub(r'__asm__\s+volatile\("sync".*?\);', "", quad_src)

    with tempfile.TemporaryDirectory(prefix="xbox360-quad-select-") as tmp:
        work = pathlib.Path(tmp)
        (work / "xsdk").mkdir()
        (work / "xsdk/kernel.h").write_text("#pragma once\n#include <cstdint>\nusing NTSTATUS = int32_t;\n#define NT_SUCCESS(s) ((s) >= 0)\n")
        (work / "xsdk/shader.h").write_text(
            """#pragma once
#include <cstdint>
#include "kernel.h"
enum { XSDK_SHADER_VERTEX = 0, XSDK_SHADER_PIXEL = 1 };
enum { XSDK_USAGE_POSITION = 0, XSDK_USAGE_TEXCOORD = 1 };
enum { XSDK_VERTEX_FLOAT2 = 0, XSDK_VERTEX_FLOAT4 = 1 };
typedef struct { uint32_t id; } XsdkShader;
typedef struct { uint32_t usage, format, offset_bytes; } XsdkVertexElement;
inline NTSTATUS xsdk_shader_load(XsdkShader *s, const void *, uint32_t bytes) {
    static uint32_t next_id = 1;
    s->id = bytes ? next_id++ : 0;
    return 0;
}
inline void xsdk_shader_free(XsdkShader *s) { s->id = 0; }
inline NTSTATUS xsdk_shader_set_vertex_layout(XsdkShader *, const XsdkVertexElement *, uint32_t, const uint32_t *) { return 0; }
""",
            encoding="utf-8",
        )
        (work / "xsdk/texture.h").write_text(
            """#pragma once
#include <cstdint>
#include "kernel.h"
enum { XSDK_TEXTURE_MAX_SIZE = 4096 };
enum { XSDK_FORMAT_R8G8B8A8 = 0 };
enum { XSDK_FILTER_POINT = 0, XSDK_FILTER_LINEAR = 1 };
enum { XSDK_ADDRESS_CLAMP = 0 };
typedef struct { int w, h; } XsdkTexture;
typedef struct { uint32_t mag_filter, min_filter, mip_filter, address_u, address_v; } XsdkSampler;
typedef struct { uint32_t dummy; } XsdkSamplerState;
inline NTSTATUS xsdk_texture_create(XsdkTexture *t, uint32_t, int w, int h, int, bool) { t->w = w; t->h = h; return 0; }
inline void xsdk_texture_upload(XsdkTexture *, int, const void *, int) {}
inline void xsdk_texture_free(XsdkTexture *) {}
inline void xsdk_texture_sampler_state(XsdkSamplerState *, const XsdkSampler *) {}
""",
            encoding="utf-8",
        )
        (work / "xsdk/gpu.h").write_text(
            """#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include "kernel.h"
#include "shader.h"
#include "texture.h"
enum { XSDK_BLEND_ZERO = 0, XSDK_BLEND_ONE = 1, XSDK_BLEND_SRC_ALPHA = 2, XSDK_BLEND_ONE_MINUS_SRC_ALPHA = 3, XSDK_BLEND_OP_ADD = 0 };
enum { XSDK_WRITE_ALL = 0xF, XSDK_CULL_NONE = 0, XSDK_PRIMITIVE_TRIANGLE_LIST = 4 };
typedef struct { uint32_t color_source, color_dest, alpha_source, alpha_dest, color_op, alpha_op, write_mask; } XsdkBlend;
typedef struct { uint32_t kind; } XsdkBlendState;
typedef struct { uint32_t dummy; } XsdkDepthStencil;
typedef struct { uint32_t dummy; } XsdkDepthStencilState;
typedef struct { uint32_t cull; } XsdkRaster;
typedef struct { uint32_t dummy; } XsdkRasterState;
struct MockGpuDrawTrace {
    uint32_t vertexShaderId = 0;
    uint32_t pixelShaderId = 0;
    uint32_t pushCount = 0;
    float pushValues[12]{};
    uint32_t literalWrites = 0;
    uint32_t blendColorSource = 999;
};
inline std::vector<MockGpuDrawTrace> &mockDrawTraces() {
    static std::vector<MockGpuDrawTrace> v;
    return v;
}
inline MockGpuDrawTrace &mockPendingDraw() {
    static MockGpuDrawTrace pending;
    return pending;
}
inline volatile uint32_t *xsdk_gpu_alloc(uint32_t bytes) {
    auto *p = new uint32_t[(bytes + 3) / 4]();
    return p;
}
inline void xsdk_gpu_free(volatile uint32_t *p) { delete[] const_cast<uint32_t *>(p); }
inline uint32_t xsdk_gpu_address(volatile const uint32_t *p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
inline void xsdk_gpu_invalidate(uint32_t, uint32_t) {}
inline uint32_t xsdk_gpu_fence() { return 1; }
inline bool xsdk_gpu_fence_done(uint32_t) { return true; }
inline NTSTATUS xsdk_gpu_wait(uint32_t, uint32_t) { return 0; }
inline void xsdk_gpu_set_shaders(const XsdkShader *vs, const XsdkShader *ps) {
    mockPendingDraw().vertexShaderId = vs ? vs->id : 0;
    mockPendingDraw().pixelShaderId = ps ? ps->id : 0;
    mockPendingDraw().literalWrites = 0;
}
inline void xsdk_gpu_set_pixel_constants(uint32_t reg, const float *values, uint32_t count) {
    if (reg == 0) {
        mockPendingDraw().pushCount = count;
        std::memcpy(mockPendingDraw().pushValues, values, sizeof(float) * 4 * count);
    } else {
        mockPendingDraw().literalWrites += count;
    }
}
inline void xsdk_gpu_set_vertex_constants(uint32_t, const float *, uint32_t) {}
inline void xsdk_gpu_blend_state(XsdkBlendState *out, const XsdkBlend *b) { out->kind = b->color_source; }
inline void xsdk_gpu_set_blend(const XsdkBlendState *b) { mockPendingDraw().blendColorSource = b ? b->kind : 999; }
inline void xsdk_gpu_depth_stencil_state(XsdkDepthStencilState *, const XsdkDepthStencil *) {}
inline void xsdk_gpu_set_depth_stencil(const XsdkDepthStencilState *) {}
inline void xsdk_gpu_raster_state(XsdkRasterState *, const XsdkRaster *) {}
inline void xsdk_gpu_set_raster(const XsdkRasterState *) {}
inline void xsdk_gpu_set_texture(uint32_t, const XsdkTexture *, const XsdkSamplerState *) {}
inline void xsdk_gpu_set_vertices(uint32_t, uint32_t, uint32_t) {}
inline void xsdk_gpu_draw(uint32_t, uint32_t) { mockDrawTraces().push_back(mockPendingDraw()); }
""",
            encoding="utf-8",
        )
        (work / "log.hpp").write_text("#pragma once\n#include <string>\nnamespace Log { inline void logError(const std::string &) {} }\n")
        (work / "image.hpp").write_text(
            """#pragma once
#include <cstdint>
enum ImageFormat { IMAGE_FORMAT_NONE, IMAGE_FORMAT_RGBA32 };
struct ImageSubrect { int x = 0, y = 0, w = 0, h = 0; };
struct ImageRenderParams {
    float x = 0, y = 0, scale = 1;
    bool centered = true;
    float opacity = 1;
    int brightness = 0;
    float rotation = 0;
    bool flip = false;
    bool blend = true;
    ImageSubrect *subrect = nullptr;
    float colorEffect = 0, fisheyeEffect = 0, whirlEffect = 0, pixelateEffect = 0, mosaicEffect = 0;
};
struct ImageData { int width = 0, height = 0; ImageFormat format = IMAGE_FORMAT_NONE; int pitch = 0; void *pixels = nullptr; float scale = 1.0f; };
""",
            encoding="utf-8",
        )
        (work / "textured_quad_stub.cpp").write_text(quad_src, encoding="utf-8")
        driver = work / "test_driver.cpp"
        driver.write_text(
            """#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
#include <xsdk/gpu.h>
#include "gpu_stats_xbox360.hpp"
#include "costume_shader_select_xbox360.hpp"
#include "textured_quad_xbox360.hpp"

int main() {
    // 1. Predicate boundary tests
    assert(!xbox360_shader::requiresFullCostumeShader(0.0f, 0.0f, 0.0f, 0.0f, 0.0f));
    assert(!xbox360_shader::requiresFullCostumeShader(0.001f, -0.001f, 0.0f, 0.001f, -0.001f));
    assert(xbox360_shader::requiresFullCostumeShader(0.002f, 0.0f, 0.0f, 0.0f, 0.0f));
    assert(xbox360_shader::requiresFullCostumeShader(0.0f, 10.0f, 0.0f, 0.0f, 0.0f));
    assert(xbox360_shader::requiresFullCostumeShader(0.0f, 0.0f, -15.0f, 0.0f, 0.0f));
    assert(xbox360_shader::requiresFullCostumeShader(0.0f, 0.0f, 0.0f, -90.0f, 0.0f));
    assert(xbox360_shader::requiresFullCostumeShader(0.0f, 0.0f, 0.0f, 0.0f, 100.0f));
    assert(xbox360_shader::requiresFullCostumeShader(std::numeric_limits<float>::quiet_NaN(), 0, 0, 0, 0));

    // 2. Production TexturedQuadXbox360 shader selection & fast/full switching
    xbox360GpuStats().reset();
    std::vector<uint8_t> pixels(32 * 32 * 4, 255);
    ImageData img{};
    img.width = 32;
    img.height = 32;
    img.pitch = 32 * 4;
    img.format = IMAGE_FORMAT_RGBA32;
    img.pixels = pixels.data();

    TexturedQuadXbox360 quad(img, 1280, 720, false);
    assert(xbox360GpuStats().textureUploads == 1);

    // Draw 0: ordinary sprite (fast path, alpha blend)
    ImageRenderParams p{};
    quad.render(p, 32, 32, 1280, 720);

    // Draw 1: ghost + brightness (fast path, opaque blend=false)
    p.opacity = 0.4f;
    p.brightness = -35;
    p.blend = false;
    quad.render(p, 32, 32, 1280, 720);

    // Draw 2: whirl (full path, switches shader + uploads 25 literals + 3 push regs)
    p.blend = true;
    p.whirlEffect = 90.0f;
    quad.render(p, 32, 32, 1280, 720);

    // Draw 3: back to fast path (subrect + ghost, switches back from full to fast)
    p.whirlEffect = 0.0f;
    p.brightness = 50;
    ImageSubrect sr{4, 4, 16, 16};
    p.subrect = &sr;
    quad.render(p, 32, 32, 1280, 720);

    // Draw 4: color effect (full path)
    p.subrect = nullptr;
    p.colorEffect = 75.0f;
    quad.render(p, 32, 32, 1280, 720);

    const auto &traces = mockDrawTraces();
    assert(traces.size() == 5);
    const uint32_t fastId = traces[0].pixelShaderId;
    const uint32_t fullId = traces[2].pixelShaderId;
    assert(fastId != 0 && fullId != 0 && fastId != fullId);

    // Verify Draw 0 & 1 & 3 used fast shader with 1 push register & 0 literals
    for (size_t idx : {size_t(0), size_t(1), size_t(3)}) {
        assert(traces[idx].pixelShaderId == fastId);
        assert(traces[idx].pushCount == 1);
        assert(traces[idx].literalWrites == 0);
    }
    assert(std::abs(traces[0].pushValues[0] - 1.0f) < 1e-6f);
    assert(std::abs(traces[0].pushValues[1] - 0.0f) < 1e-6f);
    assert(traces[0].blendColorSource == XSDK_BLEND_SRC_ALPHA);

    assert(std::abs(traces[1].pushValues[0] - 0.4f) < 1e-6f);
    assert(std::abs(traces[1].pushValues[1] - (-0.35f)) < 1e-6f);
    assert(traces[1].blendColorSource == XSDK_BLEND_ONE);

    assert(std::abs(traces[3].pushValues[0] - 0.4f) < 1e-6f);
    assert(std::abs(traces[3].pushValues[1] - 0.50f) < 1e-6f);

    // Verify Draw 2 & 4 used full shader with 3 push registers & 25 literals
    for (size_t idx : {size_t(2), size_t(4)}) {
        assert(traces[idx].pixelShaderId == fullId);
        assert(traces[idx].pushCount == 3);
        assert(traces[idx].literalWrites == 25);
    }
    assert(std::abs(traces[2].pushValues[5] - 90.0f) < 1e-6f);
    assert(std::abs(traces[4].pushValues[2] - 75.0f) < 1e-6f);

    // Verify telemetry counters
    const auto &stats = xbox360GpuStats();
    assert(stats.drawCalls == 5);
    assert(stats.fastQuadDraws == 3);
    assert(stats.fullQuadDraws == 2);
    assert(stats.shaderSwitches == 5);
    assert(stats.constantUploads == 5);
    assert(stats.constantRegisters == 3 * 1 + 2 * 3);
    assert(stats.literalUploads == 2);
    assert(stats.literalRegisters == 50);
    assert(stats.stateChanges == 15);
    assert(stats.textureBindings == 5);
    return 0;
}
""",
            encoding="utf-8",
        )
        exe = work / "test_quad_select"
        subprocess.run(
            [
                "c++",
                "-std=c++17",
                "-I",
                str(work),
                "-I",
                str(ROOT / "include/renderers/xbox360"),
                str(work / "textured_quad_stub.cpp"),
                str(driver),
                "-o",
                str(exe),
            ],
            check=True,
        )
        subprocess.run([str(exe)], check=True)


def main() -> int:
    full_blob, full_literals = load_shader("costume_frag")
    fast_blob, fast_literals = load_shader("costume_fast_frag")

    dis_full, full_slots, full_regs = inspect_disasm(full_blob)
    dis_fast, fast_slots, fast_regs = inspect_disasm(fast_blob)

    assert "dp4" not in dis_full, "Unexpected dp4 instruction in vec2 costume shader disassembly"
    assert "dp4" not in dis_fast, "Unexpected dp4 instruction in fast costume shader disassembly"
    assert not fast_literals, f"Expected 0 literals in fast shader, got {len(fast_literals)}"
    assert fast_slots <= 8 and fast_slots < full_slots, (fast_slots, full_slots)
    assert fast_regs <= 2 and fast_regs < full_regs, (fast_regs, full_regs)

    pts = [
        (0.5, 0.5),
        (0.50002, 0.5),
        (0.6, 0.5),
        (0.75, 0.6),
        (0.2, 0.2),
        (0.95, 0.95),
        (0.05, 0.05),
        (0.99, 0.5),
        (-0.01, 0.5),
        (1.01, 0.5),
    ]
    cases = [
        ("baseline", (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 0.0)),
        ("whirl_+180", (1.0, 0.0, 0.0, 0.0), (0.0, 180.0, 0.0, 0.0)),
        ("whirl_-180", (1.0, 0.0, 0.0, 0.0), (0.0, -180.0, 0.0, 0.0)),
        ("whirl_+360", (1.0, 0.0, 0.0, 0.0), (0.0, 360.0, 0.0, 0.0)),
        ("fisheye_+100", (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 100.0, 0.0)),
        ("fisheye_+200", (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 200.0, 0.0)),
        ("fisheye_-50", (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, -50.0, 0.0)),
        ("fisheye_-100", (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, -100.0, 0.0)),
        ("fisheye_-150", (1.0, 0.0, 0.0, 0.0), (0.0, 0.0, -150.0, 0.0)),
        ("whirl90+fish50", (1.0, 0.0, 0.0, 0.0), (0.0, 90.0, 50.0, 0.0)),
        ("all_combined", (0.7, 0.15, 50.0, 20.0), (20.0, 120.0, 80.0, 0.0)),
    ]

    def tfetch(slot, coord):
        return [coord[0], coord[1], 0.4, 0.8]

    overall_err = 0.0
    for zw in [(0.0, 1.0), (0.0, 0.0)]:
        for label, fx, dist in cases:
            consts = dict(full_literals)
            consts[0] = list(fx)
            consts[1] = list(dist)
            consts[2] = [64.0, 64.0, 0.0, 0.0]
            regs = [{0: [u, v, zw[0], zw[1]]} for u, v in pts]
            results = run_group(full_blob, regs, constants=consts, texture_fetch=tfetch)
            for (u, v), res in zip(pts, results):
                rk, rrgba = ref_frag(u, v, fx, dist)
                assert res.killed == rk, f"{label} zw={zw} ({u},{v}): sim kill={res.killed} != ref {rk}"
                if not res.killed:
                    err = max(abs(a - b) for a, b in zip(res.exports[0], rrgba))
                    overall_err = max(overall_err, err)
                    assert err < 1e-4, f"{label} zw={zw} ({u},{v}): err={err}"

    # Fast-path simulation parity against both full shader and Python reference across opacity/brightness/texels
    fast_pts = [(0.0, 0.0), (0.25, 0.75), (0.5, 0.5), (0.99, 0.01), (1.0, 1.0)]
    fast_cases = [
        (1.0, 0.0),
        (0.5, 0.0),
        (0.0, 0.0),
        (1.0, 0.35),
        (1.0, -0.45),
        (0.75, 1.0),
        (0.25, -1.0),
    ]
    fast_checks = 0
    fast_err = 0.0
    for opacity, brightness in fast_cases:
        fast_consts = {0: [opacity, brightness, 0.0, 0.0]}
        full_consts = dict(full_literals)
        full_consts[0] = [opacity, brightness, 0.0, 0.0]
        full_consts[1] = [0.0, 0.0, 0.0, 0.0]
        full_consts[2] = [64.0, 64.0, 0.0, 0.0]
        regs = [{0: [u, v, 0.0, 1.0]} for u, v in fast_pts]
        fast_results = run_group(fast_blob, regs, constants=fast_consts, texture_fetch=tfetch)
        full_results = run_group(full_blob, regs, constants=full_consts, texture_fetch=tfetch)
        for (u, v), f_res, full_res in zip(fast_pts, fast_results, full_results):
            assert not f_res.killed and not full_res.killed
            rk, rrgba = ref_frag(u, v, (opacity, brightness, 0.0, 0.0))
            assert not rk
            err_ref = max(abs(a - b) for a, b in zip(f_res.exports[0], rrgba))
            err_full = max(abs(a - b) for a, b in zip(f_res.exports[0], full_res.exports[0]))
            fast_err = max(fast_err, err_ref, err_full)
            assert err_ref < 1e-5 and err_full < 1e-5, (u, v, opacity, brightness, err_ref, err_full)
            fast_checks += 1

    run_host_quad_selection_tests()

    print(
        f"PASS: shader comparison full=({full_slots} slots, {full_regs} GPRs, {len(full_literals)} lits) "
        f"vs fast=({fast_slots} slots, {fast_regs} GPRs, {len(fast_literals)} lits); "
        f"220 full + {fast_checks} fast wavefront checks (max_err={max(overall_err, fast_err):.6f}); "
        f"host selection & switching OK"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
