#pragma once
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#ifdef __3DS__
#include <3ds.h>
#endif
#ifdef WII
#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
#endif

namespace Log {
void log(std::string message, bool printToScreen = true);
void logWarning(std::string message, bool printToScreen = true);
void logError(std::string message, bool printToScreen = true);
void writeToFile(std::string message);
} // namespace Log

class Timer {
  private:
#if defined(__NDS__) || defined(WII) || defined(__PS4__)
    uint64_t startTime;
#else
    std::chrono::high_resolution_clock::time_point startTime;
#endif

  public:
    Timer();
    /**
     * Starts the clock.
     */
    void start();
    /**
     * Gets the amount of time passed in milliseconds.
     * @return time passed (in ms)
     */
    int getTimeMs();
    /**
     * Checks if enough time, in milliseconds, has passed since the timer started.
     * @return True if enough time has passed, False otherwise.
     */
    bool hasElapsed(int ms);
    /**
     * Checks if enough time, in milliseconds, has passed since the timer started, and automatically restarts if true.
     * @return True if enough time has passed, False otherwise.
     */
    bool hasElapsedAndRestart(int ms);
};

class OS {
  public:
    /**
     * Gets the drive prefix for the current platform.
     * @return The string of the current platform's drive prefix. (e.g. "sdmc:" on 3DS)
     **/
    static std::string getFilesystemRootPrefix();

    /**
     * Gets the location of the current device's Scratch data folder.
     * This is where the user should put their .sb3 Scratch projects.
     * This is also where all the extra data of the app lies (custom controls data, etc).
     * @return The string of the current device's Scratch data folder.
     */
    static std::string getScratchFolderLocation();

    /**
     * Gets the location of the `RomFS`, the embedded filesystem within the executable.
     * This function should be used whenever you need to load an asset from say, the `gfx` folder.
     * @return The location of the RomFS. On OGC, Switch, Wii U, and 3DS, this is `romfs:/`. everywhere else will be an empty string.
     */
    static std::string getRomFSLocation();
    /**
     * Get the current platform that's running the app.
     * @return The string of the current platform. `3DS`, `Wii`, etc.
     */
    static std::string getPlatform();
    /**
     * Function to detect whether the platform is a New 3DS.
     * @return `true` on New 3DS, `false` everywhere else.
     */
    static bool isNew3DS();

    /**
     * Function to detect whether the platform is a DSi.
     * @return `true` on DSi, `false` everywhere else.
     */
    static bool isDSi();

    /**
     * Create a directory.
     * @param path The path of the directory to create.
     */
    static void createDirectory(const std::string &path);

    /**
     * Remove a directory recursively.
     * @param path The path of the directory to remove.
     */
    static void removeDirectory(const std::string &path);

    /**
     * Check if a file exists.
     * @param path The path of the file to check.
     * @return `true` if the file exists, `false` otherwise.
     */
    static bool fileExists(const std::string &path);

    /**
     * Get the parent path of a given path.
     * @param path The path of the file or directory.
     * @return The parent path of the given path.
     */
    static std::string parentPath(const std::string &path);

    class FilesystemError : public std::runtime_error {
      protected:
        std::string path1;
        std::string path2;
        int errorCode;

      public:
        /**
         * Construct a FilesystemError with a message and single path.
         * @param message The error message.
         * @param p The path involved in the error.
         * @param ec The system error code (errno).
         */
        FilesystemError(const std::string &message, const std::string &p, int ec = 0)
            : std::runtime_error(message), path1(p), path2(""), errorCode(ec) {}

        /**
         * Construct a FilesystemError with a message and two paths.
         * @param message The error message.
         * @param p1 The first path involved in the error.
         * @param p2 The second path involved in the error.
         * @param ec The system error code (errno).
         */
        FilesystemError(const std::string &message, const std::string &p1, const std::string &p2, int ec = 0)
            : std::runtime_error(message), path1(p1), path2(p2), errorCode(ec) {}

        /**
         * Get the first path involved in the error.
         * @return The first path.
         */
        const std::string &path() const noexcept { return path1; }

        /**
         * Get the first path involved in the error.
         * @return The first path.
         */
        const std::string &getPath1() const noexcept { return path1; }

        /**
         * Get the second path involved in the error.
         * @return The second path.
         */
        const std::string &getPath2() const noexcept { return path2; }

        /**
         * Get the system error code.
         * @return The error code.
         */
        int getErrorCode() const noexcept { return errorCode; }

        virtual ~FilesystemError() = default;
    };

    /**
     * Exception thrown when a directory does not exist.
     */
    class DirectoryNotFound : public FilesystemError {
      public:
        DirectoryNotFound(const std::string &p, int ec = 0)
            : FilesystemError("Directory not found: " + p, p, ec) {}
    };

    /**
     * Exception thrown when a file does not exist.
     */
    class FileNotFound : public FilesystemError {
      public:
        FileNotFound(const std::string &p, int ec = 0)
            : FilesystemError("File not found: " + p, p, ec) {}
    };

    /**
     * Exception thrown when a path is not a directory.
     */
    class NotADirectory : public FilesystemError {
      public:
        NotADirectory(const std::string &p, int ec = 0)
            : FilesystemError("Path is not a directory: " + p, p, ec) {}
    };

    /**
     * Exception thrown when a path is not a file.
     */
    class NotAFile : public FilesystemError {
      public:
        NotAFile(const std::string &p, int ec = 0)
            : FilesystemError("Path is not a file: " + p, p, ec) {}
    };

    /**
     * Exception thrown when failing to create a directory.
     */
    class DirectoryCreationFailed : public FilesystemError {
      public:
        DirectoryCreationFailed(const std::string &p, int ec = 0)
            : FilesystemError("Failed to create directory: " + p, p, ec) {}
    };

    /**
     * Exception thrown when failing to remove a directory.
     */
    class DirectoryRemovalFailed : public FilesystemError {
      public:
        DirectoryRemovalFailed(const std::string &p, int ec = 0)
            : FilesystemError("Failed to remove directory: " + p, p, ec) {}
    };

    /**
     * Exception thrown when failing to remove a file.
     */
    class FileRemovalFailed : public FilesystemError {
      public:
        FileRemovalFailed(const std::string &p, int ec = 0)
            : FilesystemError("Failed to remove file: " + p, p, ec) {}
    };

    /**
     * Exception thrown when failing to open a directory.
     */
    class DirectoryOpenFailed : public FilesystemError {
      public:
        DirectoryOpenFailed(const std::string &p, int ec = 0)
            : FilesystemError("Failed to open directory: " + p, p, ec) {}
    };
};
