#include "../include/wien.h"

void handle_default_def(const char* targets) {
    extern char default_targets[512];
    snprintf(default_targets, sizeof(default_targets), "%s", targets);
}