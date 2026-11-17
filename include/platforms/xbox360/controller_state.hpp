#pragma once
#include <cstdint>

namespace Xbox360 {

/// Packet-aware controller snapshot cache shared between window polling and engine input.
/// Retains the last published state when XamInputGetState reports an unchanged packet_number,
/// preventing transient same-packet reads from synthesizing false button releases.
template <typename State> class ControllerCache {
    State state_{};
    uint32_t lastStatus_ = UINT32_MAX;
    bool connected_ = false;
    bool havePacket_ = false;

  public:
    struct UpdateResult {
        bool statusChanged = false;
        uint32_t status = 0;
        bool inconsistentSamePacket = false;
        uint32_t packetNumber = 0;
        uint16_t cachedButtons = 0;
        uint16_t sampledButtons = 0;
    };

    UpdateResult update(uint32_t status, const State &sample, uint32_t successCode = 0) {
        UpdateResult result{};
        result.status = status;
        if (status != lastStatus_) {
            result.statusChanged = true;
            lastStatus_ = status;
        }
        if (status != successCode) {
            connected_ = false;
            havePacket_ = false;
            state_ = {};
            return result;
        }
        connected_ = true;
        if (!havePacket_ || sample.packet_number != state_.packet_number) {
            state_ = sample;
            havePacket_ = true;
        } else if (sample.gamepad.buttons != state_.gamepad.buttons) {
            result.inconsistentSamePacket = true;
            result.packetNumber = sample.packet_number;
            result.cachedButtons = state_.gamepad.buttons;
            result.sampledButtons = sample.gamepad.buttons;
        }
        return result;
    }

    bool connected() const { return connected_; }
    bool hasPacket() const { return havePacket_; }
    const State &state() const { return state_; }
};

} // namespace Xbox360
