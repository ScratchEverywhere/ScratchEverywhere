#pragma once
#include <nlohmann/json.hpp>

namespace SettingsManager {
void migrate();

nlohmann::json getConfigSettings();
void saveConfigSettings(const nlohmann::json &json);

nlohmann::json getProjectSettings(const std::string &projectName);
void saveProjectSettings(const nlohmann::json &json, const std::string &projectName);

bool isProjectUnpacked(const std::string &projectName);

}; // namespace SettingsManager
