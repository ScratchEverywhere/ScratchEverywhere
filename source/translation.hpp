#pragma once

#include <string>
#include <vector>

namespace TranslationManager {
struct LanguageInfo {
    unsigned int id;
    std::string key;
    std::string name;
};

const std::vector<LanguageInfo> getLanguages();
const LanguageInfo &getLoadedLanguage();

void loadLanguage(std::string language = "");

const std::string getTranslation(const std::string &translationKey);
const std::string getSplashText();
}; // namespace TranslationManager
