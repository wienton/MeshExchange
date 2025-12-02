// compiler.c
#include "../include/wien.h"

extern char default_targets[512];
extern char clean_targets[1024];

static void parse_special_block(const char* block_type, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return;

    char line[MAX_LINE_LEN];
    int in_block = 0;
    int brace_depth = 0;

    while (fgets(line, sizeof(line), file)) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        strip_comment(line);
        char* trimmed = trim(line);
        if (!trimmed || !trimmed[0]) continue;

        if (!in_block) {
            if (strcmp(block_type, "default") == 0 && strncmp(trimmed, ".default", 8) == 0) {
                in_block = 1;
                // Ищем targets = ...
            } else if (strcmp(block_type, "clean") == 0 && strncmp(trimmed, ".clean", 6) == 0) {
                in_block = 1;
            }
        } else {
            for (char* p = line; *p; p++) {
                if (*p == '{') brace_depth++;
                else if (*p == '}') brace_depth--;
            }

            if (brace_depth <= 0) {
                in_block = 0;
                brace_depth = 0;
                continue;
            }

            if (strstr(trimmed, "targets = ") == trimmed) {
                char* val = trim(trimmed + 10);
                if (block_type[0] == 'd') { // default
                    snprintf(default_targets, sizeof(default_targets), "%s", val);
                } else { // clean
                    snprintf(clean_targets, sizeof(clean_targets), "%s", val);
                }
            }
        }
    }
    fclose(file);
}

void parse_comp_blocks_from_file(const char* filename) {
    // Сначала парсим .default и .clean
    parse_special_block("default", filename);
    parse_special_block("clean", filename);

    // Теперь .comp
    FILE* file = fopen(filename, "r");
    if (!file) return;

    char line[MAX_LINE_LEN];
    int in_comp = 0;
    char current_name[MAX_NAME_LEN];
    int brace_depth = 0;

    while (fgets(line, sizeof(line), file)) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        strip_comment(line);
        char original_line[MAX_LINE_LEN];
        snprintf(original_line, sizeof(original_line), "%s", line);
        char* trimmed = trim(line);

        if (!in_comp) {
            if (strncmp(trimmed, ".comp", 5) == 0) {
                char* after = trim(trimmed + 5);
                char* brace = strchr(after, '{');
                if (brace) {
                    *brace = '\0';
                    char* name = trim(after);
                    if (name[0]) {
                        snprintf(current_name, MAX_NAME_LEN, "%s", name);
                        in_comp = 1;
                        brace_depth = 1;
                        char* rest = trim(brace + 1);
                        if (rest[0] && rest[0] != '}') {
                            if (comp_block_count < MAX_COMPS) {
                                CompileBlock* b = &comp_blocks[comp_block_count];
                                if (b->line_count == 0) {
                                    snprintf(b->name, MAX_NAME_LEN, "%s", current_name);
                                    b->defined = 1;
                                }
                                if (b->line_count < MAX_COMP_LINES) {
                                    snprintf(b->lines[b->line_count], MAX_LINE_LEN, "%s", rest);
                                    b->line_count++;
                                }
                            }
                        }
                    }
                } else {
                    char* name = after;
                    if (name[0]) {
                        snprintf(current_name, MAX_NAME_LEN, "%s", name);
                        in_comp = 1;
                        brace_depth = 0;
                    }
                }
            }
        } else {
            if (brace_depth == 0) {
                if (trimmed[0] == '{') {
                    brace_depth = 1;
                    continue;
                } else {
                    in_comp = 0;
                    continue;
                }
            }

            for (char* p = original_line; *p; p++) {
                if (*p == '{') brace_depth++;
                else if (*p == '}') brace_depth--;
            }

            if (brace_depth <= 0) {
                in_comp = 0;
                if (comp_block_count < MAX_COMPS && comp_blocks[comp_block_count].defined && comp_blocks[comp_block_count].line_count > 0) {
                    comp_block_count++;
                }
                continue;
            }

            if (strcmp(trimmed, "{") == 0 || strcmp(trimmed, "}") == 0) continue;

            if (comp_block_count < MAX_COMPS) {
                CompileBlock* b = &comp_blocks[comp_block_count];
                if (b->line_count == 0) {
                    snprintf(b->name, MAX_NAME_LEN, "%s", current_name);
                    b->defined = 1;
                }
                if (b->line_count < MAX_COMP_LINES) {
                    snprintf(b->lines[b->line_count], MAX_LINE_LEN, "%s", original_line);
                    b->line_count++;
                }
            }
        }

        if (!in_comp && comp_block_count < MAX_COMPS && comp_blocks[comp_block_count].defined && comp_blocks[comp_block_count].line_count > 0) {
            comp_block_count++;
        }
    }

    if (in_comp && comp_block_count < MAX_COMPS && comp_blocks[comp_block_count].defined && comp_blocks[comp_block_count].line_count > 0) {
        comp_block_count++;
    }

    fclose(file);
}

CompileBlock* find_comp_block(const char* name) {
    for (int i = 0; i < comp_block_count; i++) {
        if (strcmp(comp_blocks[i].name, name) == 0) {
            return &comp_blocks[i];
        }
    }
    return NULL;
}