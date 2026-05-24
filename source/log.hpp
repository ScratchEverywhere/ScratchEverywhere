#pragma once
#include <string>

namespace Log {
void log(std::string message);
void logWarning(std::string message);
void logError(std::string message);
void writeToFile(std::string message);
void deleteLogFile();
} // namespace Log