#pragma once
#include <cstdint>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

class Image;

namespace CostumeSystem {
extern std::unordered_map<std::string, std::shared_ptr<Image>> costumeImages;

void loadCurrentCostumeImage(uint32_t instanceId);

void flushCostumeImages();

void freeUnusedCostumeImages();

void clearAll();
} // namespace CostumeSystem