#pragma once
#include <se_export.hpp>

#include <string>
#include <vector>

namespace TranslationManager {
struct SE_EXPORT LanguageInfo {
    unsigned int id;
    std::string key;
    std::string name;
};

SE_EXPORT const std::vector<LanguageInfo> getLanguages();
const LanguageInfo &getLoadedLanguage();

SE_EXPORT void loadLanguage(std::string language = "");

SE_EXPORT const std::string getTranslation(const std::string &translationKey);
SE_EXPORT const std::string getSplashText();
}; // namespace TranslationManager
