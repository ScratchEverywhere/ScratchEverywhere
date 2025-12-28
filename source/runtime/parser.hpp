#pragma once
#include <nlohmann/json.hpp>
#include <sprite.hpp>

namespace Parser {

void loadUsernameFromSettings();

void loadSprites(const nlohmann::json &json);

/**
 * Gets a Chain of Blocks with a specified `blockId`.
 * @param blockId ID of the block you want the chain for.
 * @param outId a `std::string*` for if you want to get the ID of the chain. Can leave empty.
 * @return An `std::vector` of every `Block*` in the chain.
 */
std::vector<Block *> getBlockChain(std::string blockId, std::string *outID = nullptr);

#ifdef ENABLE_CLOUDVARS
void initMist();
#endif

}; // namespace Parser