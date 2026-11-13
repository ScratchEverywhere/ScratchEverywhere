#include <stddef.h>

extern int main(int argc, char **argv);

void vflash_main(void) {
    char arg0[] = "scratch-vflash";
    char *argv[] = {arg0, NULL};
    (void)main(1, argv);
    for (;;) {
    }
}
