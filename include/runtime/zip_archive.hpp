#pragma once
#include <se_export.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>

class SE_EXPORT ZipArchive {
  public:
    virtual ~ZipArchive() = default;

    virtual bool openFile(const std::string &path) = 0;
    virtual bool openMemory(const void *data, size_t size) = 0;
    virtual bool openStream(std::istream *stream, uint64_t size) = 0;

    virtual void *extractToHeap(const std::string &name, size_t *outSize) = 0;
    virtual void freeHeap(void *ptr) = 0;

    virtual bool extractAll(const std::function<std::string(const std::string &name)> &decide) = 0;
};

SE_EXPORT std::unique_ptr<ZipArchive> createZipArchive();
