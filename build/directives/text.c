// directives/text.c
#include "../include/wien.h"

void handle_text(const char* args) {
    if (!args || !args[0]) return;
    const char* v = get_var(args);
    if (v) {
        printf("%s\n", v);
    } else {
        size_t len = strlen(args);
        if (len >= 2 && args[0] == '"' && args[len - 1] == '"') {
            printf("%.*s\n", (int)(len - 2), args + 1);
        } else {
            printf("%s\n", args);
        }
    }
}