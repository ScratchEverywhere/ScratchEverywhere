#pragma once
#include <cstdint>

/// Low-overhead counters and separated CPU/GPU timings for the Xbox 360 Xenos renderer.
struct Xbox360GpuStats {
    uint64_t frames = 0;
    uint64_t drawCalls = 0;
    uint64_t fastQuadDraws = 0;
    uint64_t fullQuadDraws = 0;
    uint64_t penQuadDraws = 0;
    uint64_t penBatchDraws = 0;
    uint64_t shaderSwitches = 0;
    uint64_t constantUploads = 0;
    uint64_t constantRegisters = 0;
    uint64_t literalUploads = 0;
    uint64_t literalRegisters = 0;
    uint64_t stateChanges = 0;
    uint64_t textureBindings = 0;
    uint64_t textureUploads = 0;
    uint64_t resolves = 0;
    uint64_t gpuWaits = 0;
    uint64_t gpuWaitUs = 0;
    uint64_t cpuPrepUs = 0;
    uint64_t gpuDrawUs = 0;
    uint64_t gpuMeasuredFrames = 0;

    void reset() { *this = Xbox360GpuStats{}; }
};

inline Xbox360GpuStats &xbox360GpuStats() {
    static Xbox360GpuStats stats;
    return stats;
}
