#ifndef DBLOGS_H
#define DBLOGS_H

int  dblog_init(const char* log_path);
void dblog_close(void);

void log_info(const char* fmt, ...);
void log_warn(const char* fmt, ...);
void log_error(const char* fmt, ...);
void log_debug(const char* fmt, ...);

#endif