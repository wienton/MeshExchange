// directives/clean.c
#include "../include/wien.h"
#include <glob.h>
#include <unistd.h>

void execute_clean(void) {
    extern char clean_targets[1024];
    if (clean_targets[0] == '\0') return;

    printf("→ [CLEAN] Removing artifacts...\n");

    char* targets = strdup(clean_targets);
    char* token = strtok(targets, " \t");

    while (token) {
        glob_t gl;
        if (glob(token, GLOB_NOCHECK, NULL, &gl) == 0) {
            for (size_t i = 0; i < gl.gl_pathc; i++) {
                if (unlink(gl.gl_pathv[i]) == 0) {
                    printf("  Removed: %s\n", gl.gl_pathv[i]);
                }
            }
            globfree(&gl);
        }
        token = strtok(NULL, " \t");
    }
    free(targets);
}