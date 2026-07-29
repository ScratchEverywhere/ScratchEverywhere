#include <iostream>
#include <log.hpp>
#include <render.hpp>
#if defined(_WIN32) || defined(_WIN64) || defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__) && !defined(WEBOS)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))
#include <libdlgmod/libdlgmod.h>
#if !defined(USE_LIBDLGMOD)
#define USE_LIBDLGMOD
#endif
#endif

#if defined(__PS4__)
#include <orbis/UserService.h>
#include <orbis/SystemService.h>
#include <orbis/libkernel.h>
#endif

/**
 * See these lines, in libdlgmod's source code, for reference, where the 'BUTTON_TYPES' enum, below, is copied from:
 * https://github.com/samuelvenable/libdlgmod/blob/25d4dce0d65984a1c7dfea1ce633a4121b393f15/libdlgmod/win32/libdlgmod.cpp#L112-L120
 * https://github.com/samuelvenable/libdlgmod/blob/25d4dce0d65984a1c7dfea1ce633a4121b393f15/libdlgmod/macos/libdlgmod.mm#L49-L57
 * https://github.com/samuelvenable/libdlgmod/blob/25d4dce0d65984a1c7dfea1ce633a4121b393f15/libdlgmod/xlib/libdlgmod.cpp#L79-L87
 */
enum BUTTON_TYPES {
  BUTTON_ABORT,
  BUTTON_IGNORE,
  BUTTON_OK,
  BUTTON_CANCEL,
  BUTTON_YES,
  BUTTON_NO,
  BUTTON_RETRY
};

// PS4 implementation of logging
#ifdef __PS4__
char logBuffer[1024];
void Log::log(std::string message) {
    snprintf(logBuffer, 1023, "<SE!> %s\n", message.c_str());
    sceKernelDebugOutText(0, logBuffer);
}

void Log::logWarning(std::string message) {
    snprintf(logBuffer, 1023, "<SE!> Warning: %s\n", message.c_str());
    sceKernelDebugOutText(0, logBuffer);
}

void Log::logError(std::string message) {
    snprintf(logBuffer, 1023, "<SE!> Error: %s\n", message.c_str());
    sceKernelDebugOutText(0, logBuffer);
}

void Log::logCritical(std::string message, bool fatal) {
	if (fatal) {
    	snprintf(logBuffer, 1023, "<SE!> Fatal: %s\n", message.c_str());
	} else {
    	snprintf(logBuffer, 1023, "<SE!> Critical: %s\n", message.c_str());
	}
	sceKernelDebugOutText(0, logBuffer);
	// If fatal, exit the current SE! process instance:
	if (fatal) {
		sceSystemServiceLoadExec("exit", nullptr);
	}
}

void Log::writeToFile(std::string message) {
}

void Log::deleteLogFile() {
}
#else
static std::string lastLog;
void Log::log(std::string message) {
    if (lastLog == message) return;
    lastLog = message;
    std::cout << message << std::endl;
    writeToFile(message);
}

void Log::logWarning(std::string message) {
    if (lastLog == message) return;
    lastLog = message;
    std::cout << "\x1b[1;33m" << "Warning: " << message << "\x1b[0m" << std::endl;
    writeToFile("<Warning> " + message);
}

void Log::logError(std::string message) {
    if (lastLog == message) return;
    lastLog = message;
    std::cerr << "\x1b[1;31m" << "Error: " << message << "\x1b[0m" << std::endl;
    writeToFile("<Error> " + message);
}

/**
 * logCritical is a graphical error message
 * Has Abort-button-only when a Fatal Error
 * Adds Ignore button when not Fatal Error:
 */
void Log::logCritical(std::string message, bool fatal) {
    if (lastLog == message) return;
    lastLog = message;
	if (fatal) {
    	std::cerr << "\x1b[1;31m" << "Fatal: " << message << "\x1b[0m" << std::endl;
		writeToFile("<Fatal> " + message);
	} else {
	    std::cerr << "\x1b[1;31m" << "Critical: " << message << "\x1b[0m" << std::endl;
		writeToFile("<Critical> " + message);
	}
	#if defined(USE_LIBDLGMOD)
	// Retrieve caption text and button label strings for later use:
	const char *title = widget_get_caption();
	const char *abort = widget_get_button_name(BUTTON_ABORT);
	const char *ignore = widget_get_button_name(BUTTON_IGNORE);
	/**
	 * FIXME: Replace "Fatal Error", "Critical Error", "Abort", and "Ignore"
	 * hard-coded strings with localization support for various languages...
	 */
	if (fatal) {
		// Titlebar caption text for fatal graphical errors:
		widget_set_caption("Fatal Error");
	} else {
		// Titlebar caption text for non-fatal graphical errors:
		widget_set_caption("Critical Error");
	}
	// 'Abort' button label for all graphical errors:
	widget_set_button_name(BUTTON_ABORT, "Abort");
	// 'Ignore' button label for non-fatal graphical errors:
	widget_set_button_name(BUTTON_IGNORE, "Ignore");
	// Show the error:
	show_error(message.c_str(), fatal);
	// Reset caption text and button labels to original strings:
	widget_set_caption(title);
	widget_set_button_name(BUTTON_ABORT, abort);
	widget_set_button_name(BUTTON_IGNORE, ignore);
	#else
	// If fatal, exit the current SE! process instance:
	if (fatal) {
		exit(0);
	}
	#endif
}

void Log::writeToFile(std::string message) {
    if (Render::debugMode) {
        std::string filePath = OS::getScratchFolderLocation() + "log.txt";
        std::ofstream logFile;
        logFile.open(filePath, std::ios::app);
        if (logFile.is_open()) {
            logFile << message << std::endl;
            logFile.close();
        } else {
            std::cerr << "Could not open log file: " << filePath << std::endl;
        }
    }
}

void Log::deleteLogFile() {
    std::string filePath = OS::getScratchFolderLocation() + "/log.txt";
    if (std::remove(filePath.c_str()) != 0) {
		/**
		 * Do not use logCritical here; on some platforms it'll 
		 * always show an undesirable error on process startup.
		 */
        Log::logError("Failed to delete log file: " + std::string(std::strerror(errno)));
    }
}
#endif
