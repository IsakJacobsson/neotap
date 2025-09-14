#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_args.h"

#define DEFAULT_NUM_WORDS 10

int parse_arguments(int argc, char *argv[], args *args) {
    if (!args)
        return -1;

    // Initialize
    args->player_name = NULL;
    args->num_words = DEFAULT_NUM_WORDS;

    // Define long options
    static struct option long_options[] = {
        {"player", required_argument, 0, 'p'},
        {"num-words", required_argument, 0, 'w'},
        {0, 0, 0, 0}};

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "p:w:", long_options,
                              &option_index)) != -1) {
        switch (opt) {
        case 'p':
            args->player_name = optarg; // string
            break;
        case 'w':
            args->num_words = atoi(optarg); // convert to int
            break;
        default:
            fprintf(stderr, "Usage: %s -p <player> [-w <num-words>]\n",
                    argv[0]);
            return -1;
        }
    }

    // Check required arguments
    if (!args->player_name) {
        fprintf(stderr,
                "Missing required argument -p <player>\nUsage: %s -p <player> "
                "[-w <num-words>]\n",
                argv[0]);
        return -1;
    }

    return 0;
}
