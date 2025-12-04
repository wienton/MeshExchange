#include "dblogs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

static FILE* log_file = NULL;

#ifdef NDEBUG
#define ENABLE_DEBUG 0
#else
#define ENABLE_DEBUG 1
#endif

static void write_log(const char* level, const char* color, const char* fmt, va_list args) {
    // Get timestamp
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);

    // --- 1. Write to terminal (stderr, with color) ---
    fprintf(stderr, "%s[%s]%s %s ", color, level, "\033[0m", time_str);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);

    // --- 2. Write to log file (plain text, no color) ---
    if (log_file) {
        fprintf(log_file, "[%s] %s ", level, time_str);
        va_list args_copy;
        va_copy(args_copy, args);
        vfprintf(log_file, fmt, args_copy);
        va_end(args_copy);
        fputc('\n', log_file);
        fflush(log_file); // ensure immediate write
    }
}

int dblog_init(const char* log_path) {
    if (!log_path) return -1;
    log_file = fopen(log_path, "a");
    if (!log_file) {
        fprintf(stderr, "[ERROR] Cannot open log file: %s\n", log_path);
        return -1;
    }
    return 0;
}

void dblog_close(void) {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

void log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log("INFO", "\033[36m", fmt, args);
    va_end(args);
}

void log_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log("WARN", "\033[33m", fmt, args);
    va_end(args);
}

void log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log("ERROR", "\033[31m", fmt, args);
    va_end(args);
}

void log_debug(const char* fmt, ...) {
    if (!ENABLE_DEBUG) return;
    va_list args;
    va_start(args, fmt);
    write_log("DEBUG", "\033[35m", fmt, args);
    va_end(args);
}