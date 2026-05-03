#pragma once

#include <string>

namespace TranslationManager {
void loadLanguage(const std::string &language);

const std::string getTranslation(const std::string &translationKey);
const std::string getSplashText();
}; // namespace TranslationManager
