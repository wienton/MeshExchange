#ifndef CRYPTO_H
#define CRYPTO_H

#include <sys/stat.h>
#include <time.h>

int is_text_file(const char *path);
int encrypt_file(const char *path, mode_t mode, struct timespec mtimes[2]);

#endif