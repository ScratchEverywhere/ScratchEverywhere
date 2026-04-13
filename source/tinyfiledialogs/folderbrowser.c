#include <stdio.h>
#include <string.h>
#include "tinyfiledialogs.h"

int main(int argc, char **argv) {
    const char *lTheSelectFolderName = tinyfd_selectFolderDialog("", "");
    printf("%s\n", lTheSelectFolderName ? lTheSelectFolderName : "");
    return 0;
}

