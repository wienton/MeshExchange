#include "../include/wien.h"

static void execute_line(const char* raw_line, int line_num) {
    char line_copy[MAX_LINE_LEN];
    snprintf(line_copy, sizeof(line_copy), "%s", raw_line);

    // Удаляем комментарий
    strip_comment(line_copy);

    char* trimmed = trim(line_copy);
    if (!trimmed || !trimmed[0]) return;

    if (strncmp(trimmed, ".comp", 5) == 0) {
        // Пропускаем блоки во втором проходе
        extern int skip_mode;
        skip_mode = 1;
        return;
    }

    if (skip_mode) {
        if (strstr(trimmed, "}")) {
            skip_mode = 0;
        }
        return;
    }

    if (trimmed[0] == '.') {
        char* space = strchr(trimmed, ' ');
        char directive[32];
        char* args = "";
        if (space) {
            *space = '\0';
            args = trim(space + 1);
        }
        snprintf(directive, sizeof(directive), "%s", trimmed + 1);

        if (strcmp(directive, "text") == 0) {
            handle_text(args);
        } else if (strcmp(directive, "var") == 0) {
            handle_var(args);
        } else if (strcmp(directive, "if") == 0) {
            handle_if(args);
        } else if (strcmp(directive, "else") == 0) {
            handle_else();
        } else if (strcmp(directive, "endif") == 0) {
            handle_endif();
        } else if (strcmp(directive, "CALL") == 0) {
            handle_call(args);
        } else if (strncmp(trimmed, ".default", 8) == 0 || strncmp(trimmed, ".clean", 6) == 0) {
            skip_mode = 1;
            return;
        }
    } else {
        char* eq = strchr(trimmed, '=');
        if (eq) {
            *eq = '\0';
            char* key = trim(trimmed);
            char* val = trim(eq + 1);
            if (key[0]) set_var(key, val);
        }
    }
}

int parse_build_file(const char* filename) {
    // Проход 1: собрать .comp
    parse_comp_blocks_from_file(filename);

    // Проход 2: выполнить
    FILE* file = fopen(filename, "r");
    if (!file) return -1;

    char line[MAX_LINE_LEN];
    int line_num = 0;
    while (fgets(line, sizeof(line), file)) {
        line_num++;
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        execute_line(line, line_num);
    }
    fclose(file);
    return 0;
}