#include "translation.hpp"
#include "os.hpp"
#include "settings.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>

#ifdef USE_BUNDLE
#include <bundle_archives.hpp>
#endif

static constexpr const char *defaultLanguage = "en_us";

static std::vector<nlohmann::json> translationChain;
static std::vector<std::string> splashTexts;
static TranslationManager::LanguageInfo loadedLanguage;

static bool readAsset(const std::string &path, std::string &out) {
#ifdef USE_BUNDLE
    auto assets = Bundle::assets();
    auto found = assets.find(path);
    if (!found) return false;
    out = assets.load_text(found.value());
    return true;
#else
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
#endif
}

const TranslationManager::LanguageInfo &TranslationManager::getLoadedLanguage() {
    return loadedLanguage;
}

const std::vector<TranslationManager::LanguageInfo> TranslationManager::getLanguages() {
    std::vector<LanguageInfo> ret;

    std::string text;
    if (!readAsset(OS::getRomFSLocation() + "gfx/translations/languages.json", text)) return ret;
    nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
    if (!json.is_object()) return ret;

    for (const auto &[key, value] : json.items()) {
        LanguageInfo info{static_cast<unsigned int>(ret.size()), key, key, {}};
        if (value.is_string()) {
            info.name = value.get<std::string>();
        } else if (value.is_object()) {
            info.name = value.value("name", key);
            if (value.contains("fallbacks") && value["fallbacks"].is_array()) {
                for (const auto &fallback : value["fallbacks"]) {
                    if (fallback.is_string()) info.fallbacks.push_back(fallback.get<std::string>());
                }
            }
        }
        ret.push_back(std::move(info));
    }

    return ret;
}

void TranslationManager::loadLanguage(std::string language) {
    if (language == "") language = SettingsManager::getConfigSettings().value("Language", defaultLanguage);

    const auto languages = getLanguages();
    auto findLanguage = [&languages](const std::string &key) {
        return std::find_if(languages.begin(), languages.end(), [&key](const LanguageInfo &info) { return info.key == key; });
    };

    auto it = findLanguage(language);
    if (it == languages.end()) it = findLanguage(defaultLanguage);
    if (it != languages.end()) loadedLanguage = *it;
    else loadedLanguage = {0, language, language, {}};

    std::vector<std::string> order;
    auto visit = [&order](const std::string &key) {
        if (std::find(order.begin(), order.end(), key) == order.end()) order.push_back(key);
    };
    visit(loadedLanguage.key);
    for (size_t i = 0; i < order.size(); i++) {
        auto info = findLanguage(order[i]);
        if (info == languages.end()) continue;
        for (const auto &fallback : info->fallbacks)
            visit(fallback);
    }
    visit(defaultLanguage);

    translationChain.clear();
    splashTexts.clear();

    for (const auto &key : order) {
        const std::string base = OS::getRomFSLocation() + "gfx/translations/" + key;
        std::string text;

        if (readAsset(base + ".json", text)) {
            nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
            if (json.is_object()) translationChain.push_back(std::move(json));
        }

        if (splashTexts.empty() && readAsset(base + ".splashes.txt", text)) {
            std::istringstream stream(text);
            std::string line;
            while (std::getline(stream, line)) {
                if (!line.empty()) splashTexts.push_back(line);
            }
        }
    }
}

const std::string TranslationManager::getTranslation(const std::string &translationKey) {
    for (const auto &keys : translationChain) {
        auto it = keys.find(translationKey);
        if (it != keys.end() && it->is_string()) return it->get<std::string>();
    }
    return translationKey;
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
