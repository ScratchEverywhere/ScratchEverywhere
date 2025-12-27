#include "migrate.hpp"
#include "os.hpp"

void migrate() {
    OS::createDirectory(OS::getConfigFolderLocation());
    if (OS::getScratchFolderLocation() != OS::getConfigFolderLocation() && OS::fileExists(OS::getScratchFolderLocation() + "Settings.json")) {
        OS::renameFile(OS::getScratchFolderLocation() + "Settings.json", OS::getConfigFolderLocation() + "Settings.json");
    }
}
