#include "unpackMenu.hpp"

UnpackMenu::UnpackMenu() {
    init();
}

UnpackMenu::~UnpackMenu() {
    cleanup();
}

void UnpackMenu::init() {
    Render::renderMode = Render::BOTH_SCREENS;

    infoText = createTextObject("Please wait a moment", 200.0, 100.0);
    infoText->setScale(1.5f);
    infoText->setCenterAligned(true);
    descText = createTextObject("Do not turn off the device", 200.0, 150.0);
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

    if (infoText != nullptr) {
        delete infoText;
        infoText = nullptr;
    }

    if (descText != nullptr) {
        delete descText;
        descText = nullptr;
    }

    Render::beginFrame(0, 181, 165, 111);
    Render::beginFrame(1, 181, 165, 111);
    Render::endFrame();
    Render::renderMode = Render::BOTH_SCREENS;
}

void UnpackMenu::addToJsonArray(const std::string &filePath, const std::string &value) {
    nlohmann::json j;

    std::ifstream inFile(filePath);
    if (inFile) {
        inFile >> j;
    }
    inFile.close();

    if (!j.contains("items") || !j["items"].is_array()) {
        j["items"] = nlohmann::json::array();
    }

    j["items"].push_back(value);

    std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());

    std::ofstream outFile(filePath);
    if (!outFile) {
        std::cerr << "Failed to write JSON file: " << filePath << std::endl;
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
            result.push_back(el.get<std::string>());
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
        arr.erase(std::remove(arr.begin(), arr.end(), value), arr.end());
    }

    std::ofstream outFile(filePath);
    if (!outFile) return;
    outFile << j.dump(2);
    outFile.close();
}
