#pragma once
#include <stdbool.h>

// Struct to hold parsed arguments
typedef struct {
    char *player_name;
    int num_words;
    char *words_file;
} args;

// Parse command-line arguments
// Returns 0 on success, non-zero on failure
int parse_arguments(int argc, char *argv[], args *args);