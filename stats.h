#pragma once

#define NUM_KEYS 26 // a-z

typedef struct {
    char key;
    int pressed;
    int correct;
    double time_spent;
    double wpm_history[1000];
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

void merge_stats(stats *dest, const stats *src);

double get_key_wpm(key_stats *k);

double get_key_accuracy(key_stats *k);

void save_game_history(const char *player_name, stats *s);

void save_stats(const char *player_name, stats *s);

int load_stats(const char *player_name, stats *s);

void print_stats(const stats *s);

double calc_wpm(int total_chars, double total_time);

double calc_acc(int total_keystrokes, int correct_keystrokes);