#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
        s->per_key[i].key = 'a' + i;
        s->per_key[i].pressed = 0;
        s->per_key[i].correct = 0;
        s->per_key[i].time_spent = 0.0;
        s->per_key[i].history_len = 16; // Should grow dynamically if needed

        // Allocate initial history arrays
        s->per_key[i].wpm_history =
            malloc(sizeof(double) * s->per_key[i].history_len);
        s->per_key[i].acc_history =
            malloc(sizeof(int) * s->per_key[i].history_len);

        // zero-initialize
        if (s->per_key[i].wpm_history)
            memset(s->per_key[i].wpm_history, 0,
                   sizeof(double) * s->per_key[i].history_len);
        if (s->per_key[i].acc_history)
            memset(s->per_key[i].acc_history, 0,
                   sizeof(int) * s->per_key[i].history_len);
    }
}

// Append to dynamic array, grow if needed
static void append_history(key_stats *k, double wpm, int correct) {
    if (k->pressed >= k->history_len) {
        // Grow by 2x
        k->history_len *= 2;
        k->wpm_history =
            realloc(k->wpm_history, sizeof(double) * k->history_len);
        k->acc_history = realloc(k->acc_history, sizeof(int) * k->history_len);
    }
    k->wpm_history[k->pressed] = wpm;
    k->acc_history[k->pressed] = correct;
}

void update_key_stats(stats *s, char key_char, int correct, double time_taken) {
    if (key_char < 'a' || key_char > 'z')
        return;

    int index = key_char - 'a';

    double wpm = calc_wpm(1, time_taken);
    append_history(&s->per_key[index], wpm, correct);

    s->per_key[index].pressed++;
    s->per_key[index].correct += correct;
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

static int file_exists(const char *filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

static FILE *open_csv_with_header(const char *filename, const char *header) {
    int exists = file_exists(filename);
    FILE *f = fopen(filename, "a");
    if (!f) {
        perror("fopen");
        return NULL;
    }
    if (!exists) {
        fprintf(f, "%s\n", header);
    }
    return f;
}

void save_game_history(const char *player_name, stats *s) {
    // Get current timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char datetimebuf[20]; // "YYYY-MM-DD HH:MM:SS"
    strftime(datetimebuf, sizeof(datetimebuf), "%Y-%m-%d %H:%M:%S", t);

    // Save key-level history
    char keys_csvfile[256];
    snprintf(keys_csvfile, sizeof(keys_csvfile), "%s%s.key-history.csv",
             STATS_FILE_BASE_NAME, player_name);
    FILE *keys_csv = open_csv_with_header(keys_csvfile, "date,key,wpm,acc");
    if (keys_csv) {
        for (int i = 0; i < NUM_KEYS; i++) {
            for (int j = 0; j < s->per_key[i].pressed; j++) {
                fprintf(keys_csv, "%s,%c,%.6f,%d\n", datetimebuf,
                        s->per_key[i].key, s->per_key[i].wpm_history[j],
                        s->per_key[i].acc_history[j]);
            }
        }
        fclose(keys_csv);
    }

    // Save game-level summary
    char game_csvfile[256];
    snprintf(game_csvfile, sizeof(game_csvfile), "%s%s.game-history.csv",
             STATS_FILE_BASE_NAME, player_name);
    FILE *game_csv = open_csv_with_header(game_csvfile, "date,wpm,acc");
    if (game_csv) {
        double wpm = calc_wpm(s->total.total_keystrokes, s->total.time_spent);
        double acc =
            calc_acc(s->total.total_keystrokes, s->total.correct_keystrokes);
        fprintf(game_csv, "%s,%.6f,%.6f\n", datetimebuf, wpm, acc);
        fclose(game_csv);
    }
}

void save_stats(const char *player_name, stats *s) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s%s.overall.txt",
             STATS_FILE_BASE_NAME, player_name);

    FILE *f = fopen(filename, "w");
    if (!f) {
        return;
    }

    // Save total stats
    fprintf(f, "games_played %d\n", s->total.games_played);
    fprintf(f, "total_keystrokes %d\n", s->total.total_keystrokes);
    fprintf(f, "correct_keystrokes %d\n", s->total.correct_keystrokes);
    fprintf(f, "time_spent %lf\n", s->total.time_spent);
    fprintf(f, "best_wpm %lf\n", s->total.best_wpm);

    // Save per-key stats
    for (int i = 0; i < NUM_KEYS; i++) {
        const key_stats *k = &s->per_key[i];
        fprintf(f, "key %c pressed %d correct %d time_spent %lf\n", k->key,
                k->pressed, k->correct, k->time_spent);
    }

    fclose(f);
}

int load_stats(const char *player_name, stats *s) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s%s.overall.txt",
             STATS_FILE_BASE_NAME, player_name);

    FILE *f = fopen(filename, "r");
    if (!f) {
        return 0;
    }

    // Load total stats
    fscanf(f, "games_played %d\n", &s->total.games_played);
    fscanf(f, "total_keystrokes %d\n", &s->total.total_keystrokes);
    fscanf(f, "correct_keystrokes %d\n", &s->total.correct_keystrokes);
    fscanf(f, "time_spent %lf\n", &s->total.time_spent);
    fscanf(f, "best_wpm %lf\n", &s->total.best_wpm);

    // Load per-key stats
    for (int i = 0; i < NUM_KEYS; i++) {
        key_stats *k = &s->per_key[i];
        fscanf(f, "key %c pressed %d correct %d time_spent %lf\n", &k->key,
               &k->pressed, &k->correct, &k->time_spent);
    }

    fclose(f);

    return 1;
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