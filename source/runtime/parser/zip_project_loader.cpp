#include "zip_project_loader.hpp"
#include <istream>
#include <log.hpp>
#include <parser.hpp>
#include <runtime.hpp>
#include <settings.hpp>
#include <unzip.hpp>
#include <zip_archive.hpp>

#ifdef USE_BUNDLE
#include <bundle_archives.hpp>
#include <vector>
#endif

#if defined(__XBOX__)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <windows.h>

namespace {
// Reads a whole file straight into `dst` in large chunks.
// std::ifstream goes through the C stdio layer and its small buffer, which turns a 30+ MB read into
// tens of thousands of tiny kernel reads. Large direct reads let the file system stream the data.
bool readFileDirect(const std::string &path, char *dst, size_t size) {
    HANDLE handle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;

    constexpr size_t CHUNK_SIZE = 1024 * 1024;
    size_t done = 0;
    while (done < size) {
        const DWORD want = static_cast<DWORD>(std::min(CHUNK_SIZE, size - done));
        DWORD got = 0;
        if (!ReadFile(handle, dst + done, want, &got, nullptr) || got == 0) break;
        done += got;
    }
    CloseHandle(handle);
    return done == size;
}
} // namespace
#endif

ZipProjectLoader::ZipProjectLoader(ProjectFormat format) : format(format) {}

bool ZipProjectLoader::load(std::istream *file) {
    nlohmann::json project_json;

    if (Scratch::projectType != ProjectType::UNZIPPED) {
        auto setting = Unzip::getSetting("sb3InRam");
        bool keepInRam;
        if (setting.is_null()) {
#if defined(__NDS__) || defined(__PSP__) || defined(GAMECUBE) || defined(__XBOX__)
            keepInRam = false;
#else
            keepInRam = true;
#endif
        } else {
            keepInRam = setting.get<bool>();
        }

#if defined(__XBOX__)
        // Keeping the archive in RAM only makes sense when it fits comfortably. Larger projects fall back
        // to the file-backed archive instead of exhausting the heap.
        std::streamoff xboxProjectSize = 0;
        if (keepInRam) {
            file->clear();
            file->seekg(0, std::ios::end);
            xboxProjectSize = file->tellg();
            file->clear();
            file->seekg(0, std::ios::beg);

            const std::streamoff budget = OS::isEnhancedPlatform() ? 24 * 1024 * 1024 : 6 * 1024 * 1024;
            if (xboxProjectSize <= 0 || xboxProjectSize > budget) {
                Log::logWarning("sb3InRam disabled: project is " + std::to_string(xboxProjectSize / 1024) +
                                " KB, over the " + std::to_string(budget / 1024) + " KB in-RAM budget.");
                keepInRam = false;
            }
        }
#endif

        if (keepInRam) {
            Scratch::sb3InRam = true;

            // read the file
#if defined(__XBOX__)
            const size_t size = static_cast<size_t>(xboxProjectSize);
            const DWORD readStart = GetTickCount();

            // With Unzip::ZipBuffer's default-init allocator, this allocates without zero-filling.
            // Do NOT use reserve() + insert(): insert() fills the new elements with its value, which
            // overwrites whatever was read into the spare capacity. Hence the need for DefaultInitAllocator.
            Unzip::zipBuffer.resize(size);

            bool readOk = false;
            if (Scratch::projectType != ProjectType::EMBEDDED && !Unzip::filePath.empty()) {
                readOk = readFileDirect(Unzip::filePath, Unzip::zipBuffer.data(), size);
            }
            if (!readOk) {
                // Fallback (embedded projects, or the direct read failed): read through the stream.
                file->clear();
                file->seekg(0, std::ios::beg);
                file->read(Unzip::zipBuffer.data(), static_cast<std::streamsize>(size));
                readOk = static_cast<size_t>(file->gcount()) == size;
                file->clear();
            }
            if (!readOk) {
                Log::logCritical("Failed to read project zip buffer into memory.", false);
                Unzip::zipBuffer.clear();
                Unzip::zipBuffer.shrink_to_fit();
                return false;
            }
            Log::log("Project zip read into RAM: " + std::to_string(size / 1024) + " KB in " +
                     std::to_string(GetTickCount() - readStart) + " ms");

            // Open ZIP file.
            // the archive keeps a pointer into zipBuffer,
            // which must stay alive and unmodified.
            Unzip::zipArchive = createZipArchive();
            if (!Unzip::zipArchive->openMemory(Unzip::zipBuffer.data(), Unzip::zipBuffer.size())) {
                Log::logCritical("Failed to open project zip buffer memory archive.", false);
                Unzip::zipArchive.reset();
                Unzip::zipBuffer.clear();
                Unzip::zipBuffer.shrink_to_fit();
                return false;
            }

            // extract project.json
            size_t json_size;
            void *json_data = Unzip::zipArchive->extractToHeap("project.json", &json_size);
            if (!json_data) {
                Log::logCritical("Failed to extract project.json from zip memory archive.", false);
                return false;
            }

            project_json = nlohmann::json::parse(std::string(static_cast<const char *>(json_data), json_size));
            Unzip::zipArchive->freeHeap(json_data);
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

            project_json = nlohmann::json::parse(std::string(static_cast<const char *>(json_data), json_size));
            Unzip::zipArchive->freeHeap(json_data);
#endif

        } else {
            Scratch::sb3InRam = false;

#if defined(__XBOX__)
            Unzip::zipBuffer.clear();
            Unzip::zipBuffer.shrink_to_fit();

            Unzip::zipArchive = createZipArchive();
            bool opened = false;
            if (Scratch::projectType != ProjectType::EMBEDDED && !Unzip::filePath.empty()) {
                opened = Unzip::zipArchive->openFile(Unzip::filePath);
            }
            if (!opened) {
                file->clear();
                file->seekg(0, std::ios::end);
                uint64_t file_size = file->tellg();
                file->clear();
                file->seekg(0, std::ios::beg);
                opened = Unzip::zipArchive->openStream(file, file_size);
            }

            if (!opened) {
                Log::logCritical("Failed to initialize project zip reader.", false);
                Unzip::zipArchive.reset();
                return false;
            }

            size_t json_size;
            void *json_data = Unzip::zipArchive->extractToHeap("project.json", &json_size);
            if (!json_data) {
                Log::logCritical("Failed to extract project.json", false);
                return false;
            }

            project_json = nlohmann::json::parse(std::string(static_cast<const char *>(json_data), json_size));
            Unzip::zipArchive->freeHeap(json_data);
#else
            file->seekg(0, std::ios::end);
            uint64_t file_size = file->tellg();
            file->seekg(0, std::ios::beg);

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
#endif
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
    // Reuse the active global archive to fetch assets efficiently on Xbox
    if (Unzip::zipArchive) {
        size_t size = 0;
        void *data = Unzip::zipArchive->extractToHeap(name, &size);

        if (!data) {
            // If the file is missing from the active archive, return nullptr immediately.
            // Do not fall through to the non-Xbox disk routine, which will fail and spam the log.
            return nullptr;
        }

        if (outSize != nullptr) *outSize = size;
        return data;
    }
#endif

    auto archive = createZipArchive();
    bool initSuccess = false;

#ifdef USE_BUNDLE
    std::vector<std::byte> projectBuffer;
    if (Scratch::projectType == ProjectType::EMBEDDED) {
        auto projectBundle = Bundle::project();
        auto entry = projectBundle.find(Unzip::filePath);
        if (entry) {
            projectBuffer = projectBundle.load(*entry);
            initSuccess = archive->openMemory(projectBuffer.data(), projectBuffer.size());
        }
    } else {
#endif
        initSuccess = archive->openFile(Unzip::filePath);
#ifdef USE_BUNDLE
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