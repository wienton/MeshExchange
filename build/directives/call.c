// directives/call.c
#include "../include/wien.h"
#include <unistd.h>
#include <glob.h>

// Подстановка переменной: если val — имя переменной → вернуть её значение
// Иначе — вернуть val как есть.
// Результат копируется в out_buffer (должен быть >= MAX_VALUE_LEN)
void expand_value(const char* val, char* out_buffer) {
    if (!val || !val[0]) {
        out_buffer[0] = '\0';
        return;
    }

    // Проверим: состоит ли val только из букв, цифр, подчёркиваний?
    int is_identifier = 1;
    for (const char* p = val; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') ||
              (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') ||
              (*p == '_'))) {
            is_identifier = 0;
            break;
        }
    }

    if (is_identifier) {
        const char* var_val = get_var(val);
        if (var_val) {
            snprintf(out_buffer, MAX_VALUE_LEN, "%s", var_val);
            return;
        }
    }

    // Если не переменная — копируем как есть
    snprintf(out_buffer, MAX_VALUE_LEN, "%s", val);
}

void handle_call(const char* name) {
    if (skip_mode) return;

    CompileBlock* block = find_comp_block(name);
    if (!block || !block->defined || block->line_count == 0) {
        fprintf(stderr, "Error: .comp block '%s' is not defined\n", name);
        return;
    }

    char cc[MAX_VALUE_LEN] = "gcc";
    char cflags[MAX_VALUE_LEN] = "";
    char sources[MAX_VALUE_LEN] = "";
    char output[MAX_VALUE_LEN] = "a.out";
    char ldflags[MAX_VALUE_LEN] = "";

    for (int i = 0; i < block->line_count; i++) {
        char line[MAX_LINE_LEN];
        snprintf(line, sizeof(line), "%s", block->lines[i]);
        strip_comment(line);
        char* trimmed = trim(line);
        if (!trimmed || !trimmed[0]) continue;

        char* eq = strchr(trimmed, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = trim(trimmed);
        char* val = trim(eq + 1);
        if (!key[0] || !val[0]) continue;

        char expanded_val[MAX_VALUE_LEN];

        if (strcmp(key, "cc") == 0) {
            expand_value(val, expanded_val);
            snprintf(cc, sizeof(cc), "%s", expanded_val);
        } else if (strcmp(key, "cflags") == 0) {
            expand_value(val, expanded_val);
            snprintf(cflags, sizeof(cflags), "%s", expanded_val);
        } else if (strcmp(key, "sources") == 0) {
            expand_value(val, expanded_val);
            snprintf(sources, sizeof(sources), "%s", expanded_val);
        } else if (strcmp(key, "output") == 0) {
            expand_value(val, expanded_val);
            snprintf(output, sizeof(output), "%s", expanded_val);
        } else if (strcmp(key, "ldflags") == 0) {
            expand_value(val, expanded_val);
            snprintf(ldflags, sizeof(ldflags), "%s", expanded_val);
        }
    }

    // Авто-исходники, если пусто
    if (sources[0] == '\0') {
        glob_t gl;
        if (glob("*.c", 0, NULL, &gl) == 0 && gl.gl_pathc > 0) {
            sources[0] = '\0';
            for (size_t i = 0; i < gl.gl_pathc && strlen(sources) < sizeof(sources) - 10; i++) {
                if (i > 0) strcat(sources, " ");
                strcat(sources, gl.gl_pathv[i]);
            }
            globfree(&gl);
        } else {
            fprintf(stderr, "❌ [%s] No source files\n", name);
            return;
        }
    }

    // Проверка пересборки
    if (!needs_rebuild(output, sources)) {
        printf("→ [%s] Up to date.\n", name);
        return;
    }

    // Компиляция
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "%s %s %s -o %s %s", cc, cflags, sources, output, ldflags);
    printf("→ [%s] Compiling → %s\n", name, output);
    int result = system(cmd);
    if (result == 0) {
        printf("✅ [%s] Built.\n", name);
    } else {
        fprintf(stderr, "❌ [%s] Failed.\n", name);
    }
}