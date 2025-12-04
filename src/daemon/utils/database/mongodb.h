#ifndef MONGODB_H
#define MONGODB_H

#define MONGO_URI "mongodb://127.0.0.1:27017"
#define MONGO_DATABASE_NAME "exchange"
#define MONGO_COLL_NAME "file_exchange"
static const char* filename_docker = "../../../../docker-compose.yml";

#include <stdio.h>

// colors
#define C_BLACK   "\033[30m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"
#define C_CYAN    "\033[36m"
#define C_WHITE   "\033[37m"
#define C_RESET   "\033[0m"

// command print with color and symbols
#define print_red(fmt, ...)     printf(C_RED fmt C_RESET, ##__VA_ARGS__)
#define print_green(fmt, ...)   printf(C_GREEN fmt C_RESET, ##__VA_ARGS__)
#define print_yellow(fmt, ...)  printf(C_YELLOW fmt C_RESET, ##__VA_ARGS__)
#define print_blue(fmt, ...)    printf(C_BLUE fmt C_RESET, ##__VA_ARGS__)
#define print_error(fmt, ...)   fprintf(stderr, C_RED "[!] " fmt C_RESET, ##__VA_ARGS__)
#define print_info(fmt, ...)    printf(C_CYAN "[i] " fmt C_RESET, ##__VA_ARGS__)
#define print_ok(fmt, ...)      printf(C_GREEN "[✓] " fmt C_RESET, ##__VA_ARGS__)

// function
int auto_start_docker();
int connect_mongo(void);

#endif