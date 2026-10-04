#include "unpackMenu.hpp"
#include "translation.hpp"
#include <algorithm>
#include <filesystem.hpp>
#include <log.hpp>

UnpackMenu::UnpackMenu() {
    init();
}

UnpackMenu::~UnpackMenu() {
    cleanup();
}

void UnpackMenu::init() {
    Render::renderMode = Render::BOTH_SCREENS;

    infoText = createTextObject(TranslationManager::getTranslation("ui.unpack.wait"), 200.0, 100.0);
    infoText->setScale(1.5f);
    infoText->setCenterAligned(true);
    descText = createTextObject(TranslationManager::getTranslation("ui.unpack.warning"), 200.0, 150.0);
    descText->setScale(0.8f);
    descText->setCenterAligned(true);
}

void UnpackMenu::render() {
    Render::beginFrame(0, 181, 165, 111);
    infoText->render(200, 110);
    descText->render(200, 140);

    Render::beginFrame(1, 181, 165, 111);

    Render::endFrame();
}

void UnpackMenu::cleanup() {
    Render::beginFrame(0, 181, 165, 111);
    Render::beginFrame(1, 181, 165, 111);
    Render::endFrame();
    Render::renderMode = Render::BOTH_SCREENS;
}

namespace {
std::string entryName(const nlohmann::json &entry) {
    if (entry.is_string()) return entry.get<std::string>();
    if (entry.is_object() && entry.contains("name") && entry["name"].is_string()) return entry["name"].get<std::string>();
    return "";
}

std::string formatToString(ProjectFormat format) {
    switch (format) {
    case ProjectFormat::SB2:
        return "sb2";
    case ProjectFormat::SB1:
        return "sb1";
    case ProjectFormat::SB3:
    default:
        return "sb3";
    }
}
} // namespace

void UnpackMenu::addToJsonArray(const std::string &filePath, const std::string &value, ProjectFormat format) {
    nlohmann::json j;

    std::ifstream inFile(filePath);
    if (inFile) {
        inFile >> j;
    }
    inFile.close();

    if (!j.contains("items") || !j["items"].is_array()) {
        j["items"] = nlohmann::json::array();
    }

    nlohmann::json entry;
    entry["name"] = value;
    entry["format"] = formatToString(format);
    j["items"].push_back(entry);

    const auto err = FileSystem::createDirectory(FileSystem::parentPath(filePath));
    if (!err.has_value()) {
        Log::logCritical("Failed to create folder: " + filePath + " : " + err.error(), false);
        return;
    }

    std::ofstream outFile(filePath);
    if (!outFile) {
        Log::logCritical("Failed to write JSON file: " + filePath, false);
        return;
    }
    outFile << j.dump(2);
    outFile.close();
}

std::vector<std::string> UnpackMenu::getJsonArray(const std::string &filePath) {
    std::vector<std::string> result;
    std::ifstream inFile(filePath);
    if (!inFile) return result;

    nlohmann::json j;
    inFile >> j;
    inFile.close();

    if (j.contains("items") && j["items"].is_array()) {
        for (const auto &el : j["items"]) {
            std::string name = entryName(el);
            if (!name.empty()) result.push_back(name);
        }
    }
    return result;
}

void UnpackMenu::removeFromJsonArray(const std::string &filePath, const std::string &value) {
    std::ifstream inFile(filePath);
    if (!inFile) return;

    nlohmann::json j;
    inFile >> j;
    inFile.close();

    if (j.contains("items") && j["items"].is_array()) {
        auto &arr = j["items"];
        arr.erase(std::remove_if(arr.begin(), arr.end(), [&](const nlohmann::json &el) {
                      return entryName(el) == value;
                  }),
                  arr.end());
    }

    std::ofstream outFile(filePath);
    if (!outFile) return;
    outFile << j.dump(2);
    outFile.close();
}

ProjectFormat UnpackMenu::getUnpackedFormat(const std::string &filePath, const std::string &value) {
    std::ifstream inFile(filePath);
    if (!inFile) return ProjectFormat::SB3;

    nlohmann::json j;
    inFile >> j;
    inFile.close();

    if (j.contains("items") && j["items"].is_array()) {
        for (const auto &el : j["items"]) {
            if (entryName(el) != value) continue;
            if (el.is_object() && el.contains("format") && el["format"].is_string()) {
                const std::string &fmt = el["format"].get<std::string>();
                if (fmt == "sb2") return ProjectFormat::SB2;
                if (fmt == "sb1" || fmt == "sb") return ProjectFormat::SB1;
            }
            return ProjectFormat::SB3;
        }
    }
    return ProjectFormat::SB3;
}
