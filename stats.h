#pragma once

#define NUM_KEYS 26 // a-z

typedef struct {
    int pressed;
    int correct;
    double time_spent;
} key_stats;

typedef struct {
    int games_played;
    int total_keystrokes;
    int correct_keystrokes;
    double time_spent;
    double best_wpm;
} total_stats;

typedef struct {
    total_stats total;
    key_stats per_key[NUM_KEYS];
} stats;

void init_stats(stats *s);

void update_key_stats(stats *s, char key_char, int correct, double time_taken);

void update_total_stats(stats *stats, int total_keystrokes,
                        int correct_keystrokes, double time, double wpm);

double get_key_wpm(key_stats *k);

double get_key_accuracy(key_stats *k);

void save_stats(const char *filename, stats *s);

int load_stats(const char *filename, stats *s);

void print_stats(const stats *s);

double calc_wpm(int total_chars, double total_time);

double calc_acc(int total_keystrokes, int correct_keystrokes);