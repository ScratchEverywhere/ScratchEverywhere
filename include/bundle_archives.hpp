#pragma once

#ifdef USE_BUNDLE
#include <bundle.hpp>
#include <bundle_se_assets.h>
#include <bundle_se_project.h>

namespace Bundle {
inline bundle::archive assets() { return bundle::archive(bundle_se_assets_archive()); }
inline bundle::archive project() { return bundle::archive(bundle_se_project_archive()); }
} // namespace Bundle
#endif
