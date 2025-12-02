// directives/var.c
#include "../include/wien.h"

void handle_var(const char* args) {
    if (!args) return;
    char* space = strchr(args, ' ');
    if (!space) return;
    *space = '\0';
    char* name = trim(args);
    char* value = trim(space + 1);
    if (name && name[0]) {
        set_var(name, value);
    }
}