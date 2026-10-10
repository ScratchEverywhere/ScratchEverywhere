#include "zip_project_loader.hpp"
#include <istream>
#include <log.hpp>
#include <parser.hpp>
#include <runtime.hpp>
#include <settings.hpp>
#include <unzip.hpp>
#include <zip_archive.hpp>

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

ZipProjectLoader::ZipProjectLoader(ProjectFormat format) : format(format) {}

bool ZipProjectLoader::load(std::istream *file) {
    nlohmann::json project_json;

    if (Scratch::projectType != ProjectType::UNZIPPED) {
        auto setting = Unzip::getSetting("sb3InRam");
        bool keepInRam;
        if (setting.is_null()) {
#if defined(__NDS__) || defined(__PSP__) || defined(GAMECUBE)
            keepInRam = false;
#else
            keepInRam = true;
#endif
        } else {
            keepInRam = setting.get<bool>();
        }

        if (keepInRam) {
            Scratch::sb3InRam = true;

            // read the file
#if defined(__XBOX__)
            file->clear();
            std::streamsize size = file->tellg();
            if (size <= 0) {
                file->seekg(0, std::ios::end);
                size = file->tellg();
            }
            file->clear();
            file->seekg(0, std::ios::beg);

            Unzip::zipBuffer.resize(size);
            file->read(Unzip::zipBuffer.data(), size);
            if (static_cast<size_t>(file->gcount()) != static_cast<size_t>(size)) {
                Log::logCritical("Failed to read project zip buffer into memory.", false);
                return false;
            }
            file->clear();

            // open ZIP file
            Unzip::zipArchive = createZipArchive();
            if (!Unzip::zipArchive->openMemory(Unzip::zipBuffer.data(), Unzip::zipBuffer.size())) {
                Log::logCritical("Failed to open project zip buffer memory archive.", false);
                Unzip::zipArchive.reset();
                return false;
            }

            // extract project.json
            size_t json_size;
            void *json_data = Unzip::zipArchive->extractToHeap("project.json", &json_size);
            if (!json_data) {
                Log::logCritical("Failed to extract project.json from zip memory archive.", false);
                return false;
            }
#else
            std::streamsize size = file->tellg();
            file->seekg(0, std::ios::beg);
            Unzip::zipBuffer.resize(size);
            if (!file->read(Unzip::zipBuffer.data(), size)) {
                return false;
            }

            // open ZIP file
            Unzip::zipArchive = createZipArchive();
            if (!Unzip::zipArchive->openMemory(Unzip::zipBuffer.data(), Unzip::zipBuffer.size())) {
                Unzip::zipArchive.reset();
                return false;
            }

            // extract project.json
            size_t json_size;
            void *json_data = Unzip::zipArchive->extractToHeap("project.json", &json_size);
            if (!json_data) {
                return false;
            }
#endif

            project_json = nlohmann::json::parse(std::string(static_cast<const char *>(json_data), json_size));
            Unzip::zipArchive->freeHeap(json_data);
        } else {
            Scratch::sb3InRam = false;

#if defined(__XBOX__)
            file->clear();
            file->seekg(0, std::ios::end);
            uint64_t file_size = file->tellg();
            file->clear();
            file->seekg(0, std::ios::beg);
#else
            file->seekg(0, std::ios::end);
            uint64_t file_size = file->tellg();
            file->seekg(0, std::ios::beg);
#endif

            auto archive = createZipArchive();
            if (!archive->openStream(file, file_size)) {
                Log::logCritical("Failed to initialize project zip reader from stream.", false);
                return false;
            }

            size_t json_size;
            void *json_data = archive->extractToHeap("project.json", &json_size);
            if (!json_data) {
                Log::logCritical("Failed to extract project.json", false);
                return false;
            }

            project_json = nlohmann::json::parse(std::string(static_cast<const char *>(json_data), json_size));
            archive->freeHeap(json_data);
        }
    } else {
#if defined(__XBOX__)
        file->clear();
#endif
        file->seekg(0, std::ios::beg);

        // get file size
        file->seekg(0, std::ios::end);
        std::streamsize size = file->tellg();
#if defined(__XBOX__)
        file->clear();
#endif
        file->seekg(0, std::ios::beg);

        // put file into string
        std::string json_content;
        json_content.reserve(size);
        json_content.assign(std::istreambuf_iterator<char>(*file),
                            std::istreambuf_iterator<char>());

        project_json = nlohmann::json::parse(json_content);
    }

    if (project_json.empty()) {
        Log::logCritical("Project.json is empty.", false);
        return false;
    }

    Scratch::hasNativeExtensions = Parser::loadExtensions(project_json);
    Parser::loadSprites(project_json, format);
    return true;
}

void *ZipProjectLoader::getAsset(const std::string &name, size_t *outSize) {
#if defined(__XBOX__)
    if (Unzip::zipArchive) {
        size_t size = 0;
        void *data = Unzip::zipArchive->extractToHeap(name, &size);
        if (data) {
            if (outSize != nullptr) *outSize = size;
            return data;
        }
    }
#endif

    auto archive = createZipArchive();
    bool initSuccess = false;

#ifdef USE_CMAKERC
    if (Scratch::projectType == ProjectType::EMBEDDED) {
        const auto &fs = cmrc::romfs::get_filesystem();
#if defined(__XBOX__)
        std::string cmrcPath = OS::normalizeCMRCPath(Unzip::filePath);
        const auto &romfsFile = fs.open(cmrcPath);
#else
        const auto &romfsFile = fs.open(Unzip::filePath);
#endif
        initSuccess = archive->openMemory(romfsFile.begin(), romfsFile.size());
    } else {
#endif
        initSuccess = archive->openFile(Unzip::filePath);
#ifdef USE_CMAKERC
    }
#endif
    if (!initSuccess) {
        Log::logWarning("Failed to open project archive: " + Unzip::filePath);
        return nullptr;
    }

    size_t size = 0;
    void *data = archive->extractToHeap(name, &size);
    if (!data) {
        Log::logWarning("File not found in project archive: " + name);
        return nullptr;
    }

    if (outSize != nullptr) {
        *outSize = size;
    }

    return data;
}
