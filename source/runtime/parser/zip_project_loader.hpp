#pragma once
#include "project_loader.hpp"

class ZipProjectLoader : public ProjectLoader {
  public:
    explicit ZipProjectLoader(ProjectFormat format);

    bool load(std::istream *file) override;
    void *getAsset(const std::string &name, size_t *outSize) override;

  private:
    ProjectFormat format;
};
