// directives/call.c
#include "../include/wien.h"
#include <unistd.h>
#include <glob.h>

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

        if (strcmp(key, "cc") == 0) snprintf(cc, sizeof(cc), "%s", val);
        else if (strcmp(key, "cflags") == 0) snprintf(cflags, sizeof(cflags), "%s", val);
        else if (strcmp(key, "sources") == 0) snprintf(sources, sizeof(sources), "%s", val);
        else if (strcmp(key, "output") == 0) snprintf(output, sizeof(output), "%s", val);
        else if (strcmp(key, "ldflags") == 0) snprintf(ldflags, sizeof(ldflags), "%s", val);
    }

    // Если sources пустой — попробуем *.c
    if (sources[0] == '\0') {
        glob_t gl;
        if (glob("*.c", 0, NULL, &gl) == 0 && gl.gl_pathc > 0) {
            // Собираем все .c файлы в строку
            sources[0] = '\0';
            for (size_t i = 0; i < gl.gl_pathc && strlen(sources) < sizeof(sources) - 10; i++) {
                if (i > 0) strcat(sources, " ");
                strcat(sources, gl.gl_pathv[i]);
            }
            globfree(&gl);
        } else {
            fprintf(stderr, "❌ [%s] No source files found (sources not set and no *.c in dir)\n", name);
            return;
        }
    }

    // Проверка пересборки
    if (!needs_rebuild(output, sources)) {
        printf("→ [%s] Already up to date.\n", name);
        return;
    }

    // Формируем команду
    char cmd[2048];
    int len = snprintf(cmd, sizeof(cmd), "%s %s %s -o %s %s",
        cc, cflags, sources, output, ldflags);
    
    if (len >= (int)sizeof(cmd)) {
        fprintf(stderr, "❌ [%s] Command too long\n", name);
        return;
    }

    printf("→ [%s] Compiling → %s\n", name, output);
    int result = system(cmd);
    if (result == 0) {
        printf("✅ [%s] Built successfully.\n", name);
    } else {
        fprintf(stderr, "❌ [%s] Compilation failed.\n", name);
    }
}