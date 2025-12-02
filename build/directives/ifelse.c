// directives/ifelse.c
#include "../include/wien.h"

extern int skip_mode;
extern int skip_depth;

void handle_if(const char* condition_var) {
    if (skip_mode) {
        skip_depth++;
        return;
    }
    skip_mode = !is_truthy(condition_var);
}

void handle_else(void) {
    if (skip_depth > 0) return;
    skip_mode = !skip_mode;
}

void handle_endif(void) {
    if (skip_depth > 0) {
        skip_depth--;
        return;
    }
    skip_mode = 0;
}