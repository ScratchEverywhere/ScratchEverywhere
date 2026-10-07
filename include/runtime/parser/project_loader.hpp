#pragma once
#include <cstddef>
#include <iosfwd>
#include <memory>
#include <parser.hpp>
#include <se_export.hpp>
#include <string>

class SE_EXPORT ProjectLoader {
  public:
    virtual ~ProjectLoader() = default;

    virtual bool load(std::istream *file) = 0;

    virtual void *getAsset(const std::string &name, size_t *outSize) = 0;
};

SE_EXPORT std::unique_ptr<ProjectLoader> createProjectLoader(ProjectFormat format);
