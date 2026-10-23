#include "project_loader.hpp"

std::unique_ptr<ProjectLoader> createSb3Loader();
std::unique_ptr<ProjectLoader> createSb2Loader();
std::unique_ptr<ProjectLoader> createSb1Loader();

std::unique_ptr<ProjectLoader> createProjectLoader(ProjectFormat format) {
    switch (format) {
    case ProjectFormat::SB3:
        return createSb3Loader();
    case ProjectFormat::SB2:
        return createSb2Loader();
    case ProjectFormat::SB1:
        return createSb1Loader();
    }
    return nullptr;
}
