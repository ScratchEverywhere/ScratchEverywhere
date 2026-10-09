#include <filesystem.hpp>
#include <os.hpp>

#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <lmcons.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <windows.h>
#elif defined(__HAIKU__)
#include <FindDirectory.h>
#include <Path.h>
#elif defined(__XBOX__)
#include <windows.h>
#include <hal/debug.h>
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>
#else
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>
#endif

nonstd::expected<void, std::string> FileSystem::createDirectory(const std::string &path) {
    std::string p = path;
    std::replace(p.begin(), p.end(), '\\', '/');

    size_t pos = 0;
    while ((pos = p.find('/', pos)) != std::string::npos) {
        std::string dir = p.substr(0, pos++);
        if (dir.empty()) continue;
        if (dir == OS::getFilesystemRootPrefix()) continue; // Fixes DS but hopefully doesn't negatively affect other platforms????

#ifdef __XBOX__
        std::string xbox_dir = dir;
        std::replace(xbox_dir.begin(), xbox_dir.end(), '/', '\\');
        
        // Skip drive letter root (e.g., E: or E:\)
        if (xbox_dir.size() <= 2 && xbox_dir.back() == ':') continue;
        if (xbox_dir.size() == 3 && xbox_dir[1] == ':' && xbox_dir[2] == '\\') continue;

        if (xbox_dir.back() == '\\') xbox_dir.pop_back();
        
        DWORD dwAttrib = GetFileAttributesA(xbox_dir.c_str());
        if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
            if (!CreateDirectoryA(xbox_dir.c_str(), NULL)) {
                return nonstd::make_unexpected("Failed to create directory, " + xbox_dir + ", " + std::to_string(GetLastError()));
            }
        }
#else
        struct stat st;
        if (stat(dir.c_str(), &st) != 0) {
#ifdef _WIN32
            if (_mkdir(dir.c_str()) != 0 && errno != EEXIST) {
#else
            if (mkdir(dir.c_str(), 0777) != 0 && errno != EEXIST) {
#endif
                return nonstd::make_unexpected("Failed to create directory, " + dir + ", " + std::to_string(errno));
            }
        }
#endif
    }

    return {};
}

void FileSystem::renameFile(const std::string &originalPath, const std::string &newPath) {
    rename(originalPath.c_str(), newPath.c_str());
}

nonstd::expected<void, std::string> FileSystem::removeDirectory(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return nonstd::make_unexpected("Directory not found, " + path + ", " + std::to_string(errno));
    }

    if (!(st.st_mode & S_IFDIR)) {
        return nonstd::make_unexpected("Not a directory, " + path + ", " + std::to_string(errno));
    }

#ifdef _WIN32
    std::wstring wpath(path.size(), L' ');
    wpath.resize(std::mbstowcs(&wpath[0], path.c_str(), path.size()) + 1);

    SHFILEOPSTRUCTW options = {0};
    options.wFunc = FO_DELETE;
    options.pFrom = wpath.c_str();
    options.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

    if (SHFileOperationW(&options) != 0) {
        return nonstd::make_unexpected("Directory removal failed, " + path + ", " + std::to_string(errno));
    }
#else
    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        return nonstd::make_unexpected("Directory open failed, " + path + ", " + std::to_string(errno));
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string fullPath = path + "/" + entry->d_name;

        struct stat entrySt;
        if (stat(fullPath.c_str(), &entrySt) == 0) {
            if (S_ISDIR(entrySt.st_mode)) {
                auto potentialError = removeDirectory(fullPath);
                if (!potentialError.has_value()) return nonstd::make_unexpected(potentialError.error());
            } else {
                if (remove(fullPath.c_str()) != 0) {
                    return nonstd::make_unexpected("File removal failed, " + fullPath + ", " + std::to_string(errno));
                }
            }
        }
    }

    closedir(dir);

    if (rmdir(path.c_str()) != 0) {
        return nonstd::make_unexpected("Directory removal failed, " + path + ", " + std::to_string(errno));
    }
#endif

    return {};
}

bool FileSystem::fileExists(const std::string &path) {
#ifdef __XBOX__
    std::string xbox_path = path;
    std::replace(xbox_path.begin(), xbox_path.end(), '/', '\\');
    DWORD dwAttrib = GetFileAttributesA(xbox_path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES);
#else
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
#endif
}

std::string FileSystem::parentPath(const std::string &path) {
    size_t pos = path.find_last_of("/\\");
    if (std::string::npos != pos)
        return path.substr(0, pos);
    return "";
}

nonstd::expected<std::vector<std::string>, std::string> FileSystem::listDirectory(const std::string &path) {
    std::vector<std::string> files;

#if defined(_WIN32)
    std::string searchPath = path;
    if (searchPath.empty()) {
        searchPath = ".";
    }
    if (searchPath.back() != '/' && searchPath.back() != '\\') {
        searchPath += "/*";
    } else {
        searchPath += "*";
    }

    std::wstring wsearchPath(searchPath.size(), L' ');
    wsearchPath.resize(std::mbstowcs(&wsearchPath[0], searchPath.c_str(), searchPath.size()));

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(wsearchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return nonstd::make_unexpected("Failed to open directory, " + path + ", " + std::to_string(GetLastError()));
    }

    do {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, findData.cFileName, -1, NULL, 0, NULL, NULL);
        std::string fileName(size_needed - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, findData.cFileName, -1, &fileName[0], size_needed, NULL, NULL);

        if (fileName != "." && fileName != "..") {
            files.push_back(fileName);
        }
    } while (FindNextFileW(hFind, &findData) != 0);

    FindClose(hFind);

#elif defined(__XBOX__)
    std::string searchPath = path;
    if (searchPath.empty()) {
        searchPath = ".";
    }
    if (searchPath.back() != '/' && searchPath.back() != '\\') {
        searchPath += "\\*.*";
    } else {
        searchPath += "*.*";
    }
    std::replace(searchPath.begin(), searchPath.end(), '/', '\\');

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND || err == ERROR_NO_MORE_FILES) {
            return files;
        }
        return nonstd::make_unexpected("Failed to open directory, " + path + ", " + std::to_string(err));
    }

    do {
        std::string fileName = findData.cFileName;

#ifdef __XBOX__
        debugPrint("Found file: %s\n", findData.cFileName);
#endif

        if (fileName != "." && fileName != "..") {
            files.push_back(fileName);
        }
    } while (FindNextFileA(hFind, &findData) != 0);

    FindClose(hFind);

#else
    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        return nonstd::make_unexpected("Failed to open directory, " + path + ", " + std::to_string(errno));
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        const std::string fileName = entry->d_name;
        if (fileName != "." && fileName != "..") {
            files.push_back(fileName);
        }
    }

    closedir(dir);
#endif

    return files;
}
