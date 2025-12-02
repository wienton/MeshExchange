// sanitizer.h
#ifndef SANITIZER_H
#define SANITIZER_H

int parse_build_file(const char* filename);
const char* get_param(const char* key);  // для key=value
const char* get_var(const char* name);  

#endif