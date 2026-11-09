#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <se_export.hpp>
#include <string>

namespace NetEncoding {

SE_EXPORT std::array<uint8_t, 20> sha1(const void *data, size_t len);
SE_EXPORT std::string base64Encode(const void *data, size_t len);

} // namespace NetEncoding
