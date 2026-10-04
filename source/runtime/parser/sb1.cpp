#include "project_loader.hpp"
#include <log.hpp>
#include <memory>

namespace {
class Sb1Loader : public ProjectLoader {
  public:
    bool load(std::istream *file) override {
        Log::logCritical("Loading .sb (Scratch 1.4) projects is not supported yet.", false);
        return false;
    }

    void *getAsset(const std::string &name, size_t *outSize) override {
        return nullptr;
    }
};
} // namespace

std::unique_ptr<ProjectLoader> createSb1Loader() {
    return std::make_unique<Sb1Loader>();
}
