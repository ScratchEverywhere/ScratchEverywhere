#include "translation.hpp"
#include "os.hpp"
#include "settings.hpp"
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

static nlohmann::json translationKeys = nullptr;
static std::vector<std::string> splashTexts;
static TranslationManager::LanguageInfo loadedLanguage;

const TranslationManager::LanguageInfo &TranslationManager::getLoadedLanguage() {
    return loadedLanguage;
}

const std::vector<TranslationManager::LanguageInfo> TranslationManager::getLanguages() {
    std::vector<LanguageInfo> ret;

    const std::string path = OS::getRomFSLocation() + "gfx/translations/languages.json";
#ifdef USE_CMAKERC
    const auto &file = cmrc::romfs::get_filesystem().open(path);
    nlohmann::json json = nlohmann::json::parse(file.begin(), file.begin() + file.size());
#else
    nlohmann::json json;
    std::ifstream file(path);
    file >> json;
#endif

    for (const auto &[key, name] : json.items()) {
        ret.push_back({static_cast<unsigned int>(ret.size()), key, name});
    }

    return ret;
}

void TranslationManager::loadLanguage(std::string language) {
    if (language == "") language = SettingsManager::getConfigSettings().value("Language", "en_us");

    const auto &languages = getLanguages();
    loadedLanguage = *std::find_if(languages.begin(), languages.end(), [&language](LanguageInfo info) { return info.key == language; });

    const std::string path = OS::getRomFSLocation() + "gfx/translations/" + language + ".json";
    const std::string splashPath = OS::getRomFSLocation() + "gfx/translations/" + language + ".splashes.txt";

    splashTexts.clear();

#ifdef USE_CMAKERC
    const auto &fs = cmrc::romfs::get_filesystem();

    const auto &file = fs.open(path);
    translationKeys = nlohmann::json::parse(file.begin(), file.begin() + file.size());

    const auto &splashFile = fs.open(splashPath);
    std::string_view sv(splashFile.begin(), splashFile.size());
    std::istringstream stream{std::string(sv)};

    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            splashTexts.push_back(line);
        }
    }
#else
    std::ifstream i(path);
    i >> translationKeys;

    std::ifstream splashFile(splashPath);
    std::string line;
    while (std::getline(splashFile, line)) {
        if (!line.empty()) {
            splashTexts.push_back(line);
        }
    }
#endif
}

const std::string TranslationManager::getTranslation(const std::string &translationKey) {
    if (translationKeys.is_null() || !translationKeys.is_object() || !translationKeys.contains(translationKey)) return translationKey;
    const nlohmann::json &translation = translationKeys[translationKey];
    return translation.is_string() ? translation.get<const std::string>() : translationKey;
}

const std::string TranslationManager::getSplashText() {
    constexpr const char *fallback = "Everywhere!";

    if (splashTexts.empty()) {
        return fallback;
    }

    static std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
    std::uniform_int_distribution<size_t> dist(0, splashTexts.size() - 1);

    std::string splash = splashTexts[dist(rng)];

    // Replace {PlatformName} and {UserName} placeholders with actual values
    const std::string platformPlaceholder = "{PlatformName}";
    const std::string platform = OS::getPlatform();

    const std::string usernamePlaceholder = "{UserName}";
    std::string username = OS::getUsername();
    nlohmann::json json = SettingsManager::getConfigSettings();
    if (json.contains("EnableUsername") && json["EnableUsername"].is_boolean() && json["EnableUsername"].get<bool>()) {
        if (json.contains("Username") && json["Username"].is_string()) {
            std::string customUsername = json["Username"].get<std::string>();
            if (!customUsername.empty()) {
                username = customUsername;
            }
        }
    }

    size_t pos = 0;

    while ((pos = splash.find(platformPlaceholder, pos)) != std::string::npos) {
        splash.replace(pos, platformPlaceholder.size(), platform);
        pos += platform.size(); // move past replacement
    }

    pos = 0;
    while ((pos = splash.find(usernamePlaceholder, pos)) != std::string::npos) {
        splash.replace(pos, usernamePlaceholder.size(), username);
        pos += username.size(); // move past replacement
    }

    return splash;
}
