#ifndef MONGODB_H
#define MONGODB_H

#include <sys/types.h>
#include <time.h>

// Initialize MongoDB client (starts Docker if needed)
int mongodb_init(void);

// Insert file metadata after encryption
int mongodb_insert_file(const char *name, off_t size, mode_t mode, time_t mtime);

// Graceful shutdown
void mongodb_cleanup(void);

#endif