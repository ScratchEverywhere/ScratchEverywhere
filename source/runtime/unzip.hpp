#pragma once

#include <miniz.h>
#include <os.hpp>
#include <iosfwd>
#include <string>
#include <vector>

#ifdef ENABLE_CLOUDVARS
extern std::string projectJSON;
#endif

class Unzip {
  public:
         
    // I simply find that a "results" structure makes everything much clearer.
    template <typename T>
    struct Result {
        std::optional<T> value;
        std::string error;
        static Result success(T val) {
            return Result(val, "");
        }

        static Result failure(std::string err = "") {
            return Result(std::nullopt, err);
        }

        bool isSuccess() const {
            return value.has_value();
        }

      private:
        Result(std::optional<T> v, std::string e) : value(v), error(e) {}
    };

    static std::string loadingState;
    static std::string filePath;
    static bool UnpackedInSD;
    static std::string error;
    static bool threadFinished;



    static struct ConfigFile {
        bool loaded = false;
        std::string embeddedPath = "";
        std::string cacheFolder = "";
        std::string username = "";
        int blockUpdatesPerFrame = 1;
    } configFile;

    static nlohmann::json projectJson;

    static bool load();
    static void openScratchProject(void *arg);

    static std::string getSplashText();
    static bool deleteProjectFolder(const std::string &directory);
    static nlohmann::json getProjectSetting(const std::string &settingName);
    static std::vector<std::string> getProjectFiles(const std::string &directory);

  private:
    static void loadConfigFile();
    static Result<nlohmann::json> readProjectJson();
    static Result<std::string> openFile(const std::string &path);
    static Result<nlohmann::json> extractJson(const std::string &zipPath);
};
