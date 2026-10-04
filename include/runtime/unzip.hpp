#pragma once
#include <se_export.hpp>

#include <iosfwd>
#include <memory>
#include <optional>
#include <os.hpp>
#include <parser.hpp>
#include <project_loader.hpp>
#include <string>
#include <vector>
#include <zip_archive.hpp>

#ifdef ENABLE_CLOUDVARS
extern std::string projectJSON;
#endif

class SE_EXPORT Unzip {
  public:
    static volatile int projectOpened;
    static std::string loadingState;
    static volatile bool threadFinished;
    static std::string filePath;
    static bool UnpackedInSD;
    static std::unique_ptr<ZipArchive> zipArchive;
    static std::vector<char> zipBuffer;
    static std::unique_ptr<ProjectLoader> loader;

    static std::optional<ProjectFormat> unpackedFormatHint;

    static void openScratchProject(void *arg);
    static std::vector<std::string> getProjectFiles(const std::string &directory);
    static void *getFileInSB3(const std::string &fileName, size_t *outSize = nullptr);
    static int openFile(std::istream *&file, ProjectFormat format);
    static bool load();
    static bool extractProject(const std::string &zipPath, const std::string &destFolder);
    static bool deleteProjectFolder(const std::string &directory);
    static nlohmann::json getSetting(const std::string &settingName);

    static ProjectFormat detectFormat(const std::string &path);

    static std::string resolveZipProjectPath(const std::string &baseNoExt, ProjectFormat &format);
};
