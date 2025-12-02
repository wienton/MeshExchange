// main.c
#include "../include/wien.h"

int main(void) {
    const char* BUILD_FILE = "../../build.wien";

    if (parse_build_file(BUILD_FILE) != 0) {
        return 1;
    }

    extern char default_targets[512];
    if (default_targets[0] != '\0') {
        char* targets = strdup(default_targets);
        char* token = strtok(targets, " \t");
        while (token) {
            handle_call(token);
            token = strtok(NULL, " \t");
        }
        free(targets);
    }

    printf("Done.\n");
    return 0;
}