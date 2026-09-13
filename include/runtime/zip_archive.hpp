#pragma once
#include <se_export.hpp>

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>

/**
 * Backend-agnostic read interface over a zip/SB3 archive. Every call site
 * that used to talk to miniz directly (Image, SoundStream, Unzip) goes
 * through this instead, so swapping the zip backend (e.g. to minizip-ng)
 * only means adding a new ZipArchive implementation.
 */
class SE_EXPORT ZipArchive {
  public:
    virtual ~ZipArchive() = default;

    virtual bool openFile(const std::string &path) = 0;
    virtual bool openMemory(const void *data, size_t size) = 0;
    /**
     * Reads lazily from `stream` as files are extracted, instead of loading
     * the whole archive into memory. `stream` must outlive this ZipArchive.
     */
    virtual bool openStream(std::istream *stream, uint64_t size) = 0;

    virtual int getNumFiles() const = 0;
    /** Returns the file index, or -1 if not found. */
    virtual int locateFile(const std::string &name) const = 0;
    virtual bool getFilename(int index, std::string &outName) const = 0;

    /** Caller owns the result and must free it with freeHeap(). */
    virtual void *extractToHeap(int index, size_t *outSize) = 0;
    virtual void freeHeap(void *ptr) = 0;
    virtual bool extractToFile(int index, const std::string &outPath) = 0;
};

SE_EXPORT std::unique_ptr<ZipArchive> createZipArchive();
