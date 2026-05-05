#pragma once
#include <nonstd/expected.hpp>

namespace FileSystem {
/**
 * Create a directory.
 * @param path The path of the directory to create.
 */
nonstd::expected<void, std::string> createDirectory(const std::string &path);

/**
 * Renames/moves a file.
 * @param originalPath The original/current path of the file.
 * @param newPath The path to move/rename the file to.
 */
void renameFile(const std::string &originalPath, const std::string &newPath);

/**
 * Remove a directory recursively.
 * @param path The path of the directory to remove.
 */
nonstd::expected<void, std::string> removeDirectory(const std::string &path);

/**
 * Check if a file exists.
 * @param path The path of the file to check.
 * @return `true` if the file exists, `false` otherwise.
 */
bool fileExists(const std::string &path);

/**
 * Get the parent path of a given path.
 * @param path The path of the file or directory.
 * @return The parent path of the given path.
 */
std::string parentPath(const std::string &path);
}; // namespace FileSystem
