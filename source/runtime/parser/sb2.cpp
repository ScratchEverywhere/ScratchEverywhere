#include "parser.hpp"
#include "zip_project_loader.hpp"
#include <log.hpp>
#include <memory>

void Parser::loadSpritesSb2(const nlohmann::json &json) {
    Log::logCritical("Loading .sb2 (Scratch 2.0) projects is not supported yet.", false);
}

namespace {
class Sb2Loader : public ZipProjectLoader {
  public:
    Sb2Loader() : ZipProjectLoader(ProjectFormat::SB2) {}
};
} // namespace

std::unique_ptr<ProjectLoader> createSb2Loader() {
    return std::make_unique<Sb2Loader>();
}
