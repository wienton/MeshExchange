// directives/repeat.c
#include "../include/wien.h"

void handle_repeat(const char* args) {
    if (skip_mode) return;

    char* space = strchr(args, ' ');
    if (!space) return;

    *space = '\0';
    int count = atoi(args);
    char* command = trim(space + 1);

    if (count <= 0 || count > 100) return;

    for (int i = 0; i < count; i++) {

        // Временное решение: только .text
        if (strncmp(command, ".text ", 6) == 0) {
            handle_text(command + 6);
        }
    }
}