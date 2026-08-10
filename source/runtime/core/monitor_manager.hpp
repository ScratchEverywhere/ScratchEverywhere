#include "../core/color.hpp"
#include "../data/monitor.hpp"
#include "../entity_manager.hpp"
#include "../opcodes/opcode_registers.hpp"
#include "../vm/engine_state.hpp"
#include "vm_types.hpp"
#include <render.hpp>

struct MonitorManager {
    static void updateMonitors() {
        static VMThread monitorThread;

        for (auto &monitor : Render::monitors) {
            if (!monitor.visible) continue;

            if (monitor.isConstant) {
                continue;
            }

            if (monitor.varId != 0xFFFF) {
                monitor.value = EntityManager::variables[monitor.instanceId].orderedKeys[monitor.varId].value;
                continue;
            }

            monitorThread.pc = 0;
            monitorThread.stack.clear();
            monitorThread.callStack.clear();
            monitorThread.instanceId = monitor.instanceId;
            monitorThread.defId = monitor.instanceId;

            while (monitorThread.pc < monitor.byteCode.size()) {
                uint16_t opcode = monitor.byteCode[monitorThread.pc++];

                if (opcode == static_cast<uint16_t>(Opcode::RETURN)) break;

                BlockResult r = OpcodeRegistry::getJumpTable()[opcode](&monitorThread);

                if (r == BlockResult::YIELD_SAME) {
                    break;
                }
            }

            if (!monitorThread.stack.empty()) {
                monitor.value = monitorThread.stack.back();
            } else {
                monitor.value = Value();
            }
        }
    }
    static ColorRGBA getMonitorValueColor(const std::string &opcode) {
        if (opcode.substr(0, 5) == "data_")
            return {.r = 255, .g = 140, .b = 26, .a = 255};
        else if (opcode.substr(0, 8) == "sensing_")
            return {.r = 92, .g = 177, .b = 214, .a = 255};
        else if (opcode.substr(0, 7) == "motion_")
            return {.r = 76, .g = 151, .b = 255, .a = 255};
        else if (opcode.substr(0, 6) == "looks_")
            return {.r = 153, .g = 102, .b = 255, .a = 255};
        else if (opcode.substr(0, 6) == "sound_")
            return {.r = 207, .g = 99, .b = 207, .a = 255};
        else return {.r = 255, .g = 140, .b = 26, .a = 255};
    }
};