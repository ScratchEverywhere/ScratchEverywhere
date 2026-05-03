#include "blockUtils.hpp"
#include "downloader.hpp"
#include "math.hpp"
#include "os.hpp"
#include "runtime.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include "value.hpp"
#include <algorithm>
#include <string>
#include <unordered_map>

// We could retrieve that from the internet, but I don't know if that's a good idea and I also don't know which API we should use.
std::unordered_map<std::string, std::string> locales = {
    {"ab", "Аҧсшәа"}, {"af", "Afrikaans"}, {"ar", "العربية"}, {"am", "አማርኛ"}, {"an", "Aragonés"}, {"ast", "Asturianu"}, {"az", "Azeri"}, {"id", "Bahasa Indonesia"}, {"bn", "বাংলা"}, {"be", "Беларуская"}, {"bg", "Български"}, {"ca", "Català"}, {"cs", "Česky"}, {"cy", "Cymraeg"}, {"da", "Dansk"}, {"de", "Deutsch"}, {"et", "Eesti"}, {"el", "Ελληνικά"}, {"en", "English"}, {"es", "Español (España)"}, {"es-419", "Español Latinoamericano"}, {"eo", "Esperanto"}, {"eu", "Euskara"}, {"fa", "فارسی"}, {"fil", "Filipino"}, {"fr", "Français"}, {"fy", "Frysk"}, {"ga", "Gaeilge"}, {"gd", "Gàidhlig"}, {"gl", "Galego"}, {"ko", "한국어"}, {"ha", "Hausa"}, {"hy", "Հայերեն"}, {"he", "עִבְרִית"}, {"hi", "हिंदी"}, {"hr", "Hrvatski"}, {"xh", "isiXhosa"}, {"zu", "isiZulu"}, {"is", "Íslenska"}, {"it", "Italiano"}, {"ka", "ქართული ენა"}, {"kk", "қазақша"}, {"qu", "Kichwa"}, {"sw", "Kiswahili"}, {"ht", "Kreyòl ayisyen"}, {"ku", "Kurdî"}, {"ckb", "کوردیی ناوەندی"}, {"lv", "Latviešu"}, {"lt", "Lietuvių"}, {"hu", "Magyar"}, {"mi", "Māori"}, {"mn", "Монгол хэл"}, {"nl", "Nederlands"}, {"ja", "日本語"}, {"ja-Hira", "にほんご"}, {"nb", "Norsk Bokmål"}, {"nn", "Norsk Nynorsk"}, {"oc", "Occitan"}, {"or", "ଓଡ଼ିଆ"}, {"uz", "Oʻzbekcha"}, {"th", "ไทย"}, {"km", "ភាសាខ្មែរ"}, {"pl", "Polski"}, {"pt", "Português"}, {"pt-br", "Português Brasileiro"}, {"rap", "Rapa Nui"}, {"ro", "Română"}, {"ru", "Русский"}, {"nso", "Sepedi"}, {"tn", "Setswana"}, {"sk", "Slovenčina"}, {"sl", "Slovenščina"}, {"sr", "Српски"}, {"fi", "Suomi"}, {"sv", "Svenska"}, {"vi", "Tiếng Việt"}, {"tr", "Türkçe"}, {"uk", "Українська"}, {"zh-cn", "简体中文"}, {"zh-tw", "繁體中文"}};

std::string getCodeFromArg(const std::string &arg) {
    std::string lowerArg = arg;
    std::transform(lowerArg.begin(), lowerArg.end(), lowerArg.begin(), ::tolower);

    if (locales.find(lowerArg) != locales.end()) return lowerArg;

    for (const auto &[code, name] : locales) {
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        if (lowerName == lowerArg) return code;
    }

    return "en";
}

std::string getNameFromCode(const std::string &code) {
    auto it = locales.find(code);
    if (it != locales.end()) return it->second;
    return "English";
}

SCRATCH_SHADOW_BLOCK(translate_menu_languages, languages)

SCRATCH_BLOCK(translate, getTranslate) {
#if defined(ENABLE_DOWNLOAD)
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 0) {
        Value wordsInput, languageInput;
        if (!Scratch::getInput(block, "WORDS", thread, sprite, wordsInput) ||
            !Scratch::getInput(block, "LANGUAGE", thread, sprite, languageInput)) return BlockResult::REPEAT;

        std::string words = wordsInput.asString();
        if (std::all_of(words.begin(), words.end(), ::isdigit)) {
            *outValue = wordsInput;
            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }

        std::string langCode = getCodeFromArg(languageInput.asString());

        state->name = "https://trampoline.turbowarp.org/translate/translate?language=" + langCode + "&text=" + urlEncode(words);
        std::string tempDir = OS::getScratchFolderLocation() + "cache/";
        std::size_t h = std::hash<std::string>{}(state->name);
        std::string tempFile = tempDir + "translate_temp_" + std::to_string(h) + ".txt";

        state->completedSteps = 1;

        if (!DownloadManager::init()) return BlockResult::CONTINUE;

        // Check cache
        if (OS::fileExists(tempFile) && !DownloadManager::isDownloading(state->name)) {
            std::ifstream file(tempFile);
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            size_t start = content.find("\"result\":\"");
            if (start != std::string::npos) {
                start += 10;
                size_t end = content.find("\"", start);
                *outValue = Value(content.substr(start, end - start));
                thread->eraseState(block);
                return BlockResult::CONTINUE;
            }
        }

        DownloadManager::addDownload(state->name, tempFile);
        return BlockResult::REPEAT;
    }

    std::string tempDir = OS::getScratchFolderLocation() + "cache/";
    std::size_t h = std::hash<std::string>{}(state->name);
    std::string tempFile = tempDir + "translate_temp_" + std::to_string(h) + ".txt";

    if (DownloadManager::isDownloading(state->name)) return BlockResult::REPEAT;

    std::ifstream file(tempFile);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    size_t start = content.find("\"result\":\"");
    if (start != std::string::npos) {
        start += 10;
        size_t end = content.find("\"", start);
        *outValue = Value(content.substr(start, end - start));
    } else {
        *outValue = Value("");
    }

    thread->eraseState(block);
#else
    *outValue = Value("");
#endif
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(translate, getViewerLanguage) {
    // Uses English by default,
    // ToDo: but ready for i18n
    std::string currentLocale = "en";
    *outValue = Value(getNameFromCode(currentLocale));
    return BlockResult::CONTINUE;
}