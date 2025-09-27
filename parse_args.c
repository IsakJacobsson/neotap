#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_args.h"

#define DEFAULT_NUM_WORDS 10
#define DEFAULT_WORDS_FILE "words/words.txt"

static void print_usage(const char *prog_name) {
    fprintf(stderr,
            "Usage: %s -p <player> [options]\n\n"
            "Options:\n"
            "  -p, --player <name>           Name of the player (required)\n"
            "  -w, --num-words <N>           Number of words in the test "
            "(default: 10)\n"
            "  -f, --custom-words-file <file>  Path to custom words file\n"
            "  -h, --help                    Show this help message\n",
            prog_name);
}

int parse_arguments(int argc, char *argv[], args *args) {
    if (!args)
        return -1;

    // Initialize
    args->player_name = NULL;
    args->num_words = DEFAULT_NUM_WORDS;
    args->words_file = DEFAULT_WORDS_FILE;

    // Define long options
    static struct option long_options[] = {
        {"player", required_argument, 0, 'p'},
        {"num-words", required_argument, 0, 'w'},
        {"custom-words-file", required_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "p:w:f:h", long_options,
                              &option_index)) != -1) {
        switch (opt) {
        case 'p':
            args->player_name = optarg; // string
            break;
        case 'w':
            args->num_words = atoi(optarg); // convert to int
            break;
        case 'f':
            args->words_file = optarg; // string
            break;
        case 'h':
            print_usage(argv[0]);
            exit(0);
        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    // Check required arguments
    if (!args->player_name) {
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}
