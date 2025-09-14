#include <stdio.h>
#include <time.h>

#include "stats.h"

#define STATS_FILE_BASE_NAME "stats/"

void init_stats(stats *s) {
    s->total.games_played = 0;
    s->total.total_keystrokes = 0;
    s->total.correct_keystrokes = 0;
    s->total.time_spent = 0.0;
    s->total.best_wpm = 0.0;

    for (int i = 0; i < NUM_KEYS; i++) {
        s->per_key[i].pressed = 0;
        s->per_key[i].correct = 0;
        s->per_key[i].time_spent = 0.0;
    }
}

void update_key_stats(stats *s, char key_char, int correct, double time_taken) {
    if (key_char < 'a' || key_char > 'z')
        return;

    int index = key_char - 'a';
    s->per_key[index].pressed += 1;
    if (correct)
        s->per_key[index].correct += 1;
    s->per_key[index].time_spent += time_taken;
}

void update_total_stats(stats *stats, int total_keystrokes,
                        int correct_keystrokes, double time, double wpm) {
    stats->total.games_played++;
    stats->total.total_keystrokes += total_keystrokes;
    stats->total.correct_keystrokes += correct_keystrokes;
    stats->total.time_spent += time;

    if (wpm > stats->total.best_wpm) {
        stats->total.best_wpm = wpm;
    }
}

void merge_stats(stats *dest, const stats *src) {
    if (!dest || !src)
        return;

    // Merge totals
    dest->total.games_played += src->total.games_played;
    dest->total.total_keystrokes += src->total.total_keystrokes;
    dest->total.correct_keystrokes += src->total.correct_keystrokes;
    dest->total.time_spent += src->total.time_spent;

    // Best WPM: keep the higher one
    if (src->total.best_wpm > dest->total.best_wpm) {
        dest->total.best_wpm = src->total.best_wpm;
    }

    // Merge per-key stats
    for (int i = 0; i < NUM_KEYS; i++) {
        dest->per_key[i].pressed += src->per_key[i].pressed;
        dest->per_key[i].correct += src->per_key[i].correct;
        dest->per_key[i].time_spent += src->per_key[i].time_spent;
    }
}

void save_stats(const char *player_name, stats *s) {
    char stats_file[256];
    snprintf(stats_file, sizeof(stats_file), "%s%s.dat", STATS_FILE_BASE_NAME,
             player_name);

    FILE *f = fopen(stats_file, "wb");
    if (f) {
        fwrite(s, sizeof(stats), 1, f);
        fclose(f);
    }
}

int load_stats(const char *player_name, stats *s) {
    char stats_file[256];
    snprintf(stats_file, sizeof(stats_file), "%s%s.dat", STATS_FILE_BASE_NAME,
             player_name);

    FILE *f = fopen(stats_file, "rb");
    if (f) {
        fread(s, sizeof(stats), 1, f);
        fclose(f);
        return 1;
    }
    return 0;
}

void print_stats(const stats *s) {
    // Print total stats
    double total_acc =
        calc_acc(s->total.total_keystrokes, s->total.correct_keystrokes);
    double total_wpm = calc_wpm(s->total.total_keystrokes, s->total.time_spent);

    printf("==== TOTAL STATS ====\n");
    printf("Games played: %d\n", s->total.games_played);
    printf("Total keystrokes: %d\n", s->total.total_keystrokes);
    printf("Correct keystrokes: %d\n", s->total.correct_keystrokes);
    printf("Accuracy: %.2f%%\n", total_acc);
    printf("WPM: %.2f\n", total_wpm);
    printf("Best WPM: %.2f\n", s->total.best_wpm);

    // Print per-key stats
    printf("==== PER-KEY STATS ====\n");
    for (int i = 0; i < NUM_KEYS; i++) {
        const key_stats *k = &s->per_key[i];
        if (k->pressed > 0) { // only print used keys
            char key_char = 'a' + i;
            double acc = calc_acc(k->pressed, k->correct);
            double wpm = calc_wpm(k->pressed, k->time_spent);

            printf("Key '%c': accuracy=%.2f%%, WPM=%.2f\n", key_char, acc, wpm);
        }
    }
}

double calc_wpm(int total_chars, double total_time) {
    if (total_time <= 0.0) {
        return 0.0; // avoid division by zero
    }

    double words_typed = (double)total_chars / 5.0; // convert chars to "words"

    return words_typed / total_time * 60.0;
}

double calc_acc(int total_keystrokes, int correct_keystrokes) {
    if (total_keystrokes == 0) {
        return 0.0; // avoid divide by zero
    }
    return ((double)correct_keystrokes / total_keystrokes) * 100.0;
}