#pragma once
#include <se_export.hpp>
#include <string>

namespace Log {
SE_EXPORT void log(std::string message);
SE_EXPORT void logWarning(std::string message);
SE_EXPORT void logError(std::string message);
SE_EXPORT void logCritical(std::string message, bool fatal);
SE_EXPORT void writeToFile(std::string message);
SE_EXPORT void deleteLogFile();
} // namespace Log
