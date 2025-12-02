// directives/forloop.c
#include "../include/wien.h"

#define MAX_FOR_BODY_LINES 50
#define MAX_LINE_LEN 1024

typedef struct {
    char body_lines[MAX_FOR_BODY_LINES][MAX_LINE_LEN];
    int body_line_count;
    char var_name[MAX_NAME_LEN];
    int start;
    int end;
    int is_recording;
} ForState;

static ForState for_state = {0};

extern int skip_mode;
extern int skip_depth;


extern void execute_line(const char* line); 

void handle_for(const char* args) {
    if (skip_mode) {
        skip_depth++;
        return;
    }

    if (for_state.is_recording) {
        fprintf(stderr, ".for cannot be nested yet\n");
        skip_mode = 1;
        return;
    }

    char copy[MAX_LINE_LEN];
    snprintf(copy, sizeof(copy), "%s", args);
    char* name = trim(strtok(copy, " "));
    char* start_str = trim(strtok(NULL, " "));
    char* end_str = trim(strtok(NULL, " "));

    if (!name || !start_str || !end_str) {
        fprintf(stderr, "Usage: .for var start end\n");
        skip_mode = 1;
        return;
    }

    int start = atoi(start_str);
    int end = atoi(end_str);

    snprintf(for_state.var_name, MAX_NAME_LEN, "%s", name);
    for_state.start = start;
    for_state.end = end;
    for_state.body_line_count = 0;
    for_state.is_recording = 1;
}

void handle_endfor(void) {
    if (skip_depth > 0) {
        skip_depth--;
        return;
    }

    if (!for_state.is_recording) {
        fprintf(stderr, "Unmatched .endfor\n");
        return;
    }

    for_state.is_recording = 0;

    // Выполняем тело цикла (end - start + 1) раз
    for (int iter = for_state.start; iter <= for_state.end; iter++) {
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%d", iter);
        set_var(for_state.var_name, val_str);

        // Выполняем каждую строку тела
        for (int i = 0; i < for_state.body_line_count; i++) {
            execute_line(for_state.body_lines[i]);
        }
    }

    // Очистка
    for_state.body_line_count = 0;
}

// Вызывается из process_line, когда нужно сохранить/выполнить строку
void buffer_or_execute_line(const char* raw_line, int line_num) {
    if (for_state.is_recording) {
        if (for_state.body_line_count < MAX_FOR_BODY_LINES) {
            // Не сохраняем .endfor
            if (strncmp(trim((char*)raw_line), ".endfor", 7) != 0) {
                snprintf(for_state.body_lines[for_state.body_line_count],
                         MAX_LINE_LEN, "%s", raw_line);
                for_state.body_line_count++;
            }
        }
    } else {
        execute_line(raw_line);
    }
}