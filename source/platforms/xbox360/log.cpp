#include <cerrno>
#include <cstdio>
#include <cstring>
#include <log.hpp>
#include <os.hpp>
#include <platforms/xbox360/diagnostics.hpp>
#include <stdexcept>
#include <xsdk/log.h>

namespace {
FILE *diagnosticFile = nullptr;
const char *diagnosticPath = nullptr;
int diagnosticError = 0;
uint32_t diagnosticLines = 0;
char diagnosticOpenErrors[192] = {};
char diagnosticLastMessage[160] = {};

void writeDiagnostic(const char *level, const std::string &message) {
    snprintf(diagnosticLastMessage, sizeof diagnosticLastMessage, "%s: %s", level, message.c_str());
    xsdk_log_write("%s: %s", level, message.c_str());
    if (!diagnosticFile) return;
    errno = 0;
    const bool written = fprintf(diagnosticFile, "%s: %s\n", level, message.c_str()) >= 0;
    if (!written || fflush(diagnosticFile) != 0) {
        diagnosticError = errno ? errno : EIO;
        fclose(diagnosticFile);
        diagnosticFile = nullptr;
        return;
    }
    ++diagnosticLines;
}
} // namespace

void Xbox360Diagnostics::open() {
    if (diagnosticFile) return;
    // A distinct filename avoids sharing conflicts with the SDK's already-open log.
    // Append and flush each line so diagnostics survive abnormal termination.
    static const char *paths[] = {
        "game:/scratch-xbox360-diagnostic.log",
        "usb:/scratch-xbox360-diagnostic.log",
        "hdd:/scratch-xbox360-diagnostic.log",
    };
    diagnosticPath = nullptr;
    diagnosticError = 0;
    diagnosticLines = 0;
    diagnosticOpenErrors[0] = '\0';
    for (const char *path : paths) {
        errno = 0;
        diagnosticFile = fopen(path, "a");
        if (diagnosticFile) {
            diagnosticPath = path;
            diagnosticError = 0;
            Log::log(std::string("Diagnostic file opened: ") + path);
            if (diagnosticFile) return; // The first line must also flush successfully.
        } else diagnosticError = errno ? errno : EIO;
        diagnosticPath = nullptr;
        const size_t used = strlen(diagnosticOpenErrors);
        snprintf(diagnosticOpenErrors + used, sizeof diagnosticOpenErrors - used, "%.*s errno=%d; ",
                 static_cast<int>(strchr(path, ':') - path), path, diagnosticError);
    }
    Log::logWarning(std::string("No engine diagnostic file: ") + diagnosticOpenErrors);
}
void Xbox360Diagnostics::close() {
    if (diagnosticFile) {
        if (fclose(diagnosticFile) != 0) diagnosticError = errno ? errno : EIO;
        diagnosticFile = nullptr;
    }
}
const char *Xbox360Diagnostics::filePath() { return diagnosticPath; }
int Xbox360Diagnostics::fileError() { return diagnosticError; }
uint32_t Xbox360Diagnostics::linesWritten() { return diagnosticLines; }
const char *Xbox360Diagnostics::openErrors() { return diagnosticOpenErrors; }
const char *Xbox360Diagnostics::lastMessage() { return diagnosticLastMessage; }

void Log::log(std::string message) { writeDiagnostic("SE", message); }
void Log::logWarning(std::string message) { writeDiagnostic("SE WARNING", message); }
void Log::logError(std::string message) { writeDiagnostic("SE ERROR", message); }
void Log::logCritical(std::string message, bool fatal) {
    writeDiagnostic(fatal ? "SE FATAL" : "SE CRITICAL", message);
    if (fatal) {
        OS::toExit = true;
        throw std::runtime_error(message);
    }
}
void Log::writeToFile(std::string message) { writeDiagnostic("SE", message); }
void Log::deleteLogFile() {
    // xsdk owns the open title log. Never unlink it underneath the diagnostics writer.
    Log::logWarning("The active xsdk title log cannot be deleted through the engine logger");
}
