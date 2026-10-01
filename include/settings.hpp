#pragma once
#include <nlohmann/json.hpp>
#include <se_export.hpp>

namespace SettingsManager {
SE_EXPORT void migrate();

SE_EXPORT nlohmann::json getConfigSettings();
SE_EXPORT void saveConfigSettings(const nlohmann::json &json);

SE_EXPORT nlohmann::json getProjectSettings(const std::string &projectName);
SE_EXPORT void saveProjectSettings(const nlohmann::json &json, const std::string &projectName);

}; // namespace SettingsManager
