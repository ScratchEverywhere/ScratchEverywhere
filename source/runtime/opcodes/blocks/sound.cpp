#include "../../compiler/compiler_context.hpp"
#include "../../core/audio_manager.hpp"
#include "../../data/blueprint.hpp"
#include "../../entity_manager.hpp"
#include "../../vm/vm.hpp"
#include "../opcode_registers.hpp"
#include <algorithm>
#include <cmath>

// REGISTER_STANDARD_PARSER("sound_playuntildone", sound_playuntildone, "SOUND_MENU");
// DEFINE_EXECUTION(sound_playuntildone) {
// #ifdef ENABLE_AUDIO
//     if (thread->stack.empty()) return BlockResult::CONTINUE;
//     Value soundVal = thread->stack.back();
//     thread->stack.pop_back();
//
//     uint32_t instanceId = thread->instanceId;
//     uint32_t defId = thread->defId;
//     if (defId >= EntityManager::blueprints.size()) return BlockResult::CONTINUE;
//     const TargetDefinition &targetDef = EntityManager::blueprints[defId];
//     const auto &sounds = targetDef.sounds;
//
//     std::string soundFullName;
//     uint32_t soundStringId = 0;
//     bool soundFound = false;
//
//     if (soundVal.isString()) {
//         std::string soundName = soundVal.asString();
//         for (const Sound &snd : sounds) {
//             if (snd.name == soundName) {
//                 soundFullName = snd.fullName;
//                 soundStringId = StringPool::getOrInsert(snd.fullName);
//                 soundFound = true;
//                 break;
//             }
//         }
//     }
//
//     if (!soundFound && !sounds.empty()) {
//         if (soundVal.isNumeric() && !soundVal.isNaN()) {
//             double index = std::trunc(soundVal.asDouble());
//             double soundIndex = index - (std::floor((index - 1) / sounds.size()) * sounds.size()) - 1;
//             size_t idx = static_cast<size_t>(soundIndex);
//             if (idx < sounds.size()) {
//                 soundFullName = sounds[idx].fullName;
//                 soundStringId = StringPool::getOrInsert(sounds[idx].fullName);
//                 soundFound = true;
//             }
//         }
//     }
//
//     if (soundFound) {
//         uint32_t playbackId = AudioManager::playSound(instanceId, soundStringId, soundFullName);
//         if (playbackId != 0) {
//             thread->waitHandle = playbackId;
//             thread->state = ThreadState::WAITING_FOR_SOUND;
//             return BlockResult::YIELD;
//         }
//     }
// #endif
//     return BlockResult::CONTINUE;
// }
//
// REGISTER_STANDARD_PARSER("sound_play", sound_play, "SOUND_MENU");
// DEFINE_EXECUTION(sound_play) {
// #ifdef ENABLE_AUDIO
//     if (thread->stack.empty()) return BlockResult::CONTINUE;
//     Value soundVal = thread->stack.back();
//     thread->stack.pop_back();
//
//     uint32_t instanceId = thread->instanceId;
//     uint32_t defId = thread->defId;
//     if (defId >= EntityManager::blueprints.size()) return BlockResult::CONTINUE;
//     const TargetDefinition &targetDef = EntityManager::blueprints[defId];
//     const auto &sounds = targetDef.sounds;
//
//     std::string soundFullName;
//     uint32_t soundStringId = 0;
//     bool soundFound = false;
//
//     if (soundVal.isString()) {
//         std::string soundName = soundVal.asString();
//         for (const Sound &snd : sounds) {
//             if (snd.name == soundName) {
//                 soundFullName = snd.fullName;
//                 soundStringId = StringPool::getOrInsert(snd.fullName);
//                 soundFound = true;
//                 break;
//             }
//         }
//     }
//
//     if (!soundFound && !sounds.empty()) {
//         if (soundVal.isNumeric() && !soundVal.isNaN()) {
//             double index = std::trunc(soundVal.asDouble());
//             double soundIndex = index - (std::floor((index - 1) / sounds.size()) * sounds.size()) - 1;
//             size_t idx = static_cast<size_t>(soundIndex);
//             if (idx < sounds.size()) {
//                 soundFullName = sounds[idx].fullName;
//                 soundStringId = StringPool::getOrInsert(sounds[idx].fullName);
//                 soundFound = true;
//             }
//         }
//     }
//
//     if (soundFound) {
//         AudioManager::playSound(instanceId, soundStringId, soundFullName);
//     }
// #endif
//     return BlockResult::CONTINUE;
// }
//
// REGISTER_STANDARD_PARSER("sound_stopallsounds", sound_stopallsounds);
// DEFINE_EXECUTION(sound_stopallsounds) {
// #ifdef ENABLE_AUDIO
//     AudioManager::stopAllSounds();
// #endif
//     return BlockResult::CONTINUE;
// }
//
// REGISTER_STANDARD_PARSER("sound_setvolumeto", sound_setvolumeto, "VOLUME");
// DEFINE_EXECUTION(sound_setvolumeto) {
//     if (thread->stack.empty()) return BlockResult::CONTINUE;
//     Value volVal = thread->stack.back();
//     thread->stack.pop_back();
//     if (thread->instanceId < EntityManager::audio.size()) {
//         EntityManager::audio[thread->instanceId].volume = std::clamp(static_cast<float>(volVal.asDouble()), 0.0f, 100.0f);
//     }
//     return BlockResult::CONTINUE;
// }
//
// REGISTER_STANDARD_PARSER("sound_changevolumeby", sound_changevolumeby, "VOLUME");
// DEFINE_EXECUTION(sound_changevolumeby) {
//     if (thread->stack.empty()) return BlockResult::CONTINUE;
//     Value changeVal = thread->stack.back();
//     thread->stack.pop_back();
//     if (thread->instanceId < EntityManager::audio.size()) {
//         float currentVol = EntityManager::audio[thread->instanceId].volume;
//         EntityManager::audio[thread->instanceId].volume = std::clamp(currentVol + static_cast<float>(changeVal.asDouble()), 0.0f, 100.0f);
//     }
//     return BlockResult::CONTINUE;
// }
//
// REGISTER_STANDARD_PARSER("sound_volume", sound_volume);
// DEFINE_EXECUTION(sound_volume) {
//     float vol = 100.0f;
//     if (thread->instanceId < EntityManager::audio.size()) {
//         vol = EntityManager::audio[thread->instanceId].volume;
//     }
//     thread->stack.push_back(Value(static_cast<double>(vol)));
//     return BlockResult::CONTINUE;
// }
//