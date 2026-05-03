#include "translation.hpp"
#include "os.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

static nlohmann::json translationKeys = nullptr;

void TranslationManager::loadLanguage(const std::string &language) {
    const std::string path = OS::getRomFSLocation() + "gfx/translations/" + language + ".json";

#ifdef USE_CMAKERC
    const auto &file = cmrc::romfs::get_filesystem().open(path.c_str());
    translationKeys = nlohmann::json::parse(file.begin(), file.begin() + file.size());
#else
    std::ifstream i(path);
    i >> translationKeys;
#endif
}

const std::string TranslationManager::getTranslation(const std::string &translationKey) {
    if (translationKeys.is_null() || !translationKeys.is_object() || !translationKeys.contains(translationKey)) return translationKey;
    const nlohmann::json &translation = translationKeys[translationKey];
    return translation.is_string() ? translation.get<const std::string>() : translationKey;
}
