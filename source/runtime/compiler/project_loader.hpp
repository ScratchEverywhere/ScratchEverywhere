#pragma once
#include "../data/blueprint.hpp"
#include <nlohmann/json.hpp>

class ProjectLoader {
  public:
    static void loadProject(nlohmann::json &json);

  private:
    static void loadAdvancedProjectSettings(const nlohmann::json &json);
    static void parseTarget(const nlohmann::json &targetJson, TargetDefinition &def);
    static void parseAssets(const nlohmann::json &targetJson, TargetDefinition &def);
    static void parseVariablesAndLists(const nlohmann::json &targetJson, TargetDefinition &def);
};