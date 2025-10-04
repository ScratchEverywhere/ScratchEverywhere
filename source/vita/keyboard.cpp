#include "keyboard.hpp"
#include <psp2/sysmodule.h>
#include <psp2/ime_dialog.h>

static char mybuf[60];
static bool didit = false;

std::string Keyboard::openKeyboard(const char *hintText) {
	if (!sceSysmoduleIsLoaded(SCE_SYSMODULE_IME))
		sceSysmoduleLoadModule(SCE_SYSMODULE_IME);
	
	// TODO: figure out how to ime
	
    return "";
}
