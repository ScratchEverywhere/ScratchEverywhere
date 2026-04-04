#pragma once

#include "meta.hpp"

namespace extensions {
void loadLua(Extension *extension, std::istream &data);
void registerHandlers(Extension *extension);
void registerHandlers();
} // namespace extensions
