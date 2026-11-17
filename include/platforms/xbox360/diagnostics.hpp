#pragma once
#include <cstdint>

namespace Xbox360Diagnostics {
void open();
void close();
const char *filePath();
int fileError();
uint32_t linesWritten();
const char *openErrors();
const char *lastMessage();
} // namespace Xbox360Diagnostics
