#include "linker.hpp"

#define BUF_SIZE (64 * 1024)
#ifdef __3DS__

std::string Linker::sanitizeString(const std::string &input, bool padWithNull) {
    std::string cleaned;

    // Only allow 0x00 - 0xFF
    for (unsigned char c : input) {
        cleaned.push_back(c);
    }

    // Shorten to 20 characters
    if (cleaned.size() > 20) {
        cleaned.resize(20);
    }

    // Padding
    while (cleaned.size() < 20) {
        cleaned.push_back(padWithNull ? '\0' : ' ');
    }

    return cleaned;
}

std::vector<unsigned char> Linker::stringToHexWith00(const std::string &input) {
    std::vector<unsigned char> bytes;
    bytes.reserve(input.size() * 2);

    for (size_t i = 0; i < input.size(); i++) {
        bytes.push_back(static_cast<unsigned char>(input[i]));

        if (i < input.size() - 1) {
            bytes.push_back(0x00);
        }
    }

    return bytes;
}

bool Linker::isValidString(const std::string &str) {
    bool hasValidChar = false;

    for (unsigned char c : str) {
        if (!std::isalnum(c)) continue;

        hasValidChar = true;
        break;
    }

    return hasValidChar;
}

bool Linker::replaceInFile(const std::string &inputFile, const std::string &outputFile, const std::vector<unsigned char> &searchPattern, const std::vector<unsigned char> &replacePattern) {
    if (searchPattern.empty()) return false;
    if (searchPattern.size() != replacePattern.size()) {
        Log::logError("Search and replace pattern must have same size!");
        return false;
    }

    // Read file
    std::ifstream in(inputFile, std::ios::binary);
    if (!in) return false;

    std::vector<unsigned char> buffer(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
    in.close();

    // Find & replace
    for (size_t i = 0; i + searchPattern.size() <= buffer.size(); i++) {
        if (std::equal(searchPattern.begin(), searchPattern.end(), buffer.begin() + i)) {
            std::copy(replacePattern.begin(), replacePattern.end(), buffer.begin() + i);
        }
    }

    // Write new file
    std::ofstream out(outputFile, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
    return true;
}

bool Linker::installLinker(std::string title, std::string author, std::string romPath, std::string projectName) {

    if (!isValidString(title) || !isValidString(author)) return false;
    std::vector<unsigned char> originalTitleHex = stringToHexWith00("Link your Apps to SE");
    std::vector<unsigned char> originalAuthorHex = stringToHexWith00("Br0tcraft           ");
    std::vector<unsigned char> titleHex = stringToHexWith00(title);
    std::vector<unsigned char> authorHex = stringToHexWith00(author);


    replaceInFile(romPath, OS::getScratchFolderLocation() + "Linker/cache/LinkerCache.cia", originalTitleHex, titleHex);
    replaceInFile(OS::getScratchFolderLocation() + "Linker/cache/LinkerCache.cia", OS::getScratchFolderLocation() + "Linker/cache/LinkerCache.cia", originalAuthorHex, authorHex);

    amInit();

    romPath = (OS::getScratchFolderLocation() + "Linker/cache/LinkerCache.cia").c_str();

    FILE *in = fopen(romPath.c_str(), "rb");
    if (!in) {
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') << romPath.c_str();
        Log::logWarning("Could not open 0x" + *romPath.c_str());
        amExit();
        return false;
    }

    Handle ciaHandle;
    Result rc = AM_StartCiaInstall(MEDIATYPE_SD, &ciaHandle);
    if (R_FAILED(rc)) {
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') << rc;
        Log::logWarning("AM_StartCiaInstall failed: 0x" + oss.str());
        fclose(in);
        amExit();
        return false;
    }

    u8 *buf = (u8 *)malloc(BUF_SIZE);
    if (!buf) {
        Log::logWarning("Out of memory");
        AM_CancelCIAInstall(ciaHandle);
        fclose(in);
        amExit();
        return false;
    }

    size_t r;
    u64 offset = 0;
    while ((r = fread(buf, 1, BUF_SIZE, in)) > 0) {
        u32 written;
        rc = FSFILE_Write(ciaHandle, &written, offset, buf, r, FS_WRITE_FLUSH);
        if (R_FAILED(rc) || written != r) {
            std::ostringstream oss;
            oss << std::hex << std::setw(8) << std::setfill('0') << rc;
            Log::logWarning("Write error 0x" + oss.str());
            AM_CancelCIAInstall(ciaHandle);
            free(buf);
            fclose(in);
            amExit();
            return false;
        }
        offset += written;
    }

    free(buf);
    fclose(in);

    rc = AM_FinishCiaInstall(ciaHandle);
    if (R_SUCCEEDED(rc)) {
        Log::log("CIA installed successfully!");
    } else {
        std::ostringstream oss;
        oss << std::hex << std::setw(8) << std::setfill('0') << rc;
        Log::logWarning("Install failed: 0x" + oss.str());
    }

    amExit();
    return true;
}

#else
bool Linker::installLinker(std::string title, std::string author, std::string romPath, std::string projectName = "") {}
bool replaceInFile(const std::string &inputFile, const std::string &outputFile, const std::vector<unsigned char> &searchPattern, const std::vector<unsigned char> &replacePattern) {}
std::string Linker::sanitizeString(const std::string &input, bool padWithNull = false) {}
std::string Linker::stringToHexWith00(const std::string &input) {}
bool Linker::isValidString(const std::string &str) {}
#endif