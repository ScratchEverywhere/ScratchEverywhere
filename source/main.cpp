#include "scratch/blockExecutor.hpp"
#include "scratch/input.hpp"
#include "scratch/menus/mainMenu.hpp"
#include "scratch/render.hpp"
#include "scratch/unzip.hpp"
#include <chrono>

#ifdef __OGC__
#include <SDL2/SDL.h>
#endif

#ifdef ENABLE_CLOUDVARS
#include "scratch/os.hpp"
#include <mist/mist.hpp>
#include <random>
#include <sstream>
#endif

#if defined(__WIIU__) && defined(ENABLE_CLOUDVARS)
#include <whb/sdcard.h>
#endif

#ifdef ENABLE_CLOUDVARS
const uint64_t FNV_PRIME_64 = 1099511628211ULL;
const uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037ULL;

std::string cloudUsername;

std::string projectJSON;
extern bool cloudProject;

std::unique_ptr<MistConnection> cloudConnection = nullptr;
#endif

static void exitApp() {
    Render::deInit();
}

static bool initApp() {
    return Render::Init();
}

#ifdef ENABLE_CLOUDVARS
void initMist() {
    // Username Stuff

#ifdef __WIIU__
    std::ostringstream usernameFilenameStream;
    usernameFilenameStream << WHBGetSdCardMountPath() << "/wiiu/scratch-wiiu/cloud-username.txt";
    std::string usernameFilename = usernameFilenameStream.str();
#else
    std::string usernameFilename = "cloud-username.txt";
#endif

    std::ifstream fileStream(usernameFilename.c_str());
    if (!fileStream.good()) {
        std::random_device rd;
        std::ostringstream usernameStream;
        usernameStream << "player" << std::setw(7) << std::setfill('0') << rd() % 10000000;
        cloudUsername = usernameStream.str();
        std::ofstream usernameFile;
        usernameFile.open(usernameFilename);
        usernameFile << cloudUsername;
        usernameFile.close();
    } else {
        fileStream >> cloudUsername;
    }
    fileStream.close();

    uint64_t projectHash = FNV_OFFSET_BASIS_64;
    for (char c : projectJSON) {
        projectHash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        projectHash *= FNV_PRIME_64;
    }

    std::ostringstream projectID;
    projectID << "Scratch-3DS/hash-" << std::hex << std::setw(16) << std::setfill('0') << projectHash;
    cloudConnection = std::make_unique<MistConnection>(projectID.str(), cloudUsername, "contact@grady.link");

    cloudConnection->onConnectionStatus([](bool connected, const std::string &message) {
        if (connected) {
            Log::log("Mist++ Connected:");
            Log::log(message);
            return;
        }
        Log::log("Mist++ Disconnected:");
        Log::log(message);
    });

    cloudConnection->onVariableUpdate(BlockExecutor::handleCloudVariableChange);

#if defined(__WIIU__) || defined(__3DS__)
    cloudConnection->connect(false);
#else
    cloudConnection->connect();
#endif
}
#endif

int main(int argc, char **argv) {
    if (!initApp()) {
        exitApp();
        return 1;
    }

    if (!Unzip::load()) {

        if (Unzip::projectOpened == -3) { // main menu

            MainMenu menu;
            bool isLoaded = false;
            while (!isLoaded) {

                menu.render();
                if ((!menu.hasProjects && menu.shouldExit) || !Render::appShouldRun()) {
                    exitApp();
                    return 0;
                }

                if (Unzip::filePath != "") {
                    menu.cleanup();
                    if (!Unzip::load()) {
                        exitApp();
                        return 0;
                    }
                    isLoaded = true;
                }
            }
            if (!Render::appShouldRun()) {
                menu.cleanup();
                exitApp();
                return 0;
            }

        } else {

            exitApp();
            return 0;
        }
    }

#ifdef ENABLE_CLOUDVARS
    if (cloudProject && !projectJSON.empty()) initMist();
#endif

    BlockExecutor::runAllBlocksByOpcode(Block::EVENT_WHENFLAGCLICKED);
    BlockExecutor::timer.start();

    while (Render::appShouldRun()) {
        if (Render::checkFramerate()) {
            Input::getInput();
            BlockExecutor::runRepeatBlocks();
            BlockExecutor::runBroadcasts();
            Render::renderSprites();
        }
    }

    exitApp();
    return 0;
}
