#include <stdio.h>
#include <string.h>
#include "tinyfiledialogs.h"

int main(int argc, char **argv) {
    const char *lTheSelectFolderName = tinyfd_selectFolderDialog("Select a folder containing Scratch *.sb3 Game Files...", "");
    printf("%s", lTheSelectFolderName ? lTheSelectFolderName : "");
	if (lTheSelectFolderName && strlen(lTheSelectFolderName)) {
#ifdef _WIN32
        if (lTheSelectFolderName[strlen(lTheSelectFolderName) - 1] != '\\') {
            printf("\\");
		}
#else
        if (lTheSelectFolderName[strlen(lTheSelectFolderName) - 1] != '/') {
            printf("/");
        }
#endif
		printf("\n");
	}
    return 0;
}

