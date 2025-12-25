#pragma once

#include "interpret.hpp"
#include "miniz.h"
#include "os.hpp"
#include <iosfwd>
#include <string>
#include <vector>

#ifdef ENABLE_CLOUDVARS
extern std::string projectJSON;
#endif

class Unzip {
  public:
    static volatile int projectOpened;
    static std::string loadingState;
    static volatile bool threadFinished;
    static std::string filePath;
    static bool UnpackedInSD;
    static mz_zip_archive zipArchive;
    static std::vector<char> zipBuffer;

    static void openScratchProject(void *arg);
    static std::vector<std::string> getProjectFiles(const std::string &directory);
    static std::string getSplashText();
    static nlohmann::json unzipProject(std::istream *file);
    static int openFile(std::istream *&file);
    static bool load();
    static bool extractProject(const std::string &zipPath, const std::string &destFolder);
    static bool deleteProjectFolder(const std::string &directory);
    static nlohmann::json getSetting(const std::string &settingName);
};
