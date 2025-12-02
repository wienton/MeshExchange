// wien.h
#ifndef WIEN_H
#define WIEN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_VARS          100
#define MAX_COMPS         20
#define MAX_NAME_LEN      128
#define MAX_VALUE_LEN     512
#define MAX_LINE_LEN      1024
#define MAX_COMP_LINES    50

typedef struct {
    char name[MAX_NAME_LEN];
    char lines[MAX_COMP_LINES][MAX_LINE_LEN];
    int line_count;
    int defined;
} CompileBlock;

// Глобальные состояния 
extern char var_names[MAX_VARS][MAX_NAME_LEN];
extern char var_values[MAX_VARS][MAX_VALUE_LEN];
extern int var_count;

extern CompileBlock comp_blocks[MAX_COMPS];
extern int comp_block_count;

extern int skip_mode;
extern int skip_depth;

extern char default_targets[512];
extern char clean_targets[1024];

char* trim(char* str);
char* strip_comment(char* line);
void set_var(const char* name, const char* value);
const char* get_var(const char* name);
int is_truthy(const char* name);

// compiler.c
void parse_comp_blocks_from_file(const char* filename);
CompileBlock* find_comp_block(const char* name);

// directives
void handle_text(const char* args);
void handle_var(const char* args);
void handle_if(const char* condition_var);
void handle_else(void);
void handle_endif(void);
void handle_call(const char* name);
void handle_clean_def(const char* targets);  
void handle_default_def(const char* targets); 

// Утилиты
int file_exists(const char* path);
int needs_rebuild(const char* output, const char* sources);

// sanitaizer.c
int parse_build_file(const char* filename);

// clean
void execute_clean(void);

#endif