// context.c
#include "../include/wien.h"
#include <sys/stat.h>
#include <unistd.h>

char var_names[MAX_VARS][MAX_NAME_LEN];
char var_values[MAX_VARS][MAX_VALUE_LEN];
int var_count = 0;

CompileBlock comp_blocks[MAX_COMPS];
int comp_block_count = 0;

int skip_mode = 0;
int skip_depth = 0;

char default_targets[512] = "";
char clean_targets[1024] = "";

char* trim(char* str) {
    if (!str) return NULL;
    char* start = str;
    while (*start && isspace((unsigned char)*start)) start++;
    if (*start == '\0') return start;
    char* end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    return start;
}

char* strip_comment(char* line) {
    if (!line) return NULL;
    int in_quotes = 0;
    for (char* p = line; *p; p++) {
        if (*p == '"') in_quotes = !in_quotes;
        else if (*p == ';' && !in_quotes) {
            *p = '\0';
            break;
        }
    }
    return line;
}

void set_var(const char* name, const char* value) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_names[i], name) == 0) {
            snprintf(var_values[i], MAX_VALUE_LEN, "%s", value);
            return;
        }
    }
    if (var_count < MAX_VARS) {
        snprintf(var_names[var_count], MAX_NAME_LEN, "%s", name);
        snprintf(var_values[var_count], MAX_VALUE_LEN, "%s", value);
        var_count++;
    }
}

const char* get_var(const char* name) {
    for (int i = 0; i < var_count; i++)
        if (strcmp(var_names[i], name) == 0)
            return var_values[i];
    return NULL;
}

int is_truthy(const char* name) {
    const char* v = get_var(name);
    if (!v) return 0;
    if (v[0] == '\0') return 0;
    if (strcmp(v, "0") == 0) return 0;
    if (strcmp(v, "false") == 0) return 0;
    if (strcmp(v, "no") == 0) return 0;
    return 1;
}

int file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

int needs_rebuild(const char* output, const char* sources) {
    struct stat out_st;
    if (stat(output, &out_st) != 0) return 1; // output не существует

    char* src_copy = strdup(sources);
    char* token = strtok(src_copy, " \t");
    while (token) {
        struct stat src_st;
        if (stat(token, &src_st) == 0) {
            if (src_st.st_mtime > out_st.st_mtime) {
                free(src_copy);
                return 1;
            }
        }
        token = strtok(NULL, " \t");
    }
    free(src_copy);
    return 0;
}