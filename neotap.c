#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "parse_args.h"
#include "parse_words.h"
#include "stats.h"

static struct termios old;

static int get_terminal_width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
        return 80; // fallback
    return w.ws_col;
}

static int build_test_text(char **words, size_t num_words, char *output,
                           size_t output_size, size_t num_test_words,
                           int term_width) {
    if (!words || !output || output_size == 0 || num_words == 0)
        return 0;

    size_t current_idx = 0;
    int col = 0;       // current column in the terminal
    int nbr_lines = 1; // start with first line

    for (size_t i = 0; i < num_test_words; i++) {
        int word_idx = rand() % num_words;
        char *word = words[word_idx];
        int word_len = strlen(word);

        // Truncate word if it's too long for terminal
        if (word_len >= term_width)
            word_len = term_width - 1;

        // Add space if not first word
        if (i > 0) {
            // Wrap to new line if space + word exceeds terminal width
            if (col + 1 + word_len >= term_width) {
                // Ensure we have space for newline
                if (current_idx + 1 >= output_size)
                    break;

                // Add a space at the end of the line
                if (col < term_width && current_idx < output_size) {
                    output[current_idx++] = ' ';
                }

                output[current_idx++] = '\n';
                col = 0;
                nbr_lines++;
            } else {
                if (current_idx + 1 >= output_size)
                    break;
                output[current_idx++] = ' ';
                col += 1;
            }
        }

        // Check buffer space
        if (current_idx + word_len >= output_size)
            break;

        // Copy the word
        memcpy(&output[current_idx], word, word_len);
        current_idx += word_len;
        col += word_len;

        // Wrap if word reaches terminal width exactly
        if (col >= term_width) {
            if (current_idx + 1 < output_size) {
                output[current_idx++] = ' ';
                output[current_idx++] = '\n';
                col = 0;
                nbr_lines++;
            }
        }
    }

    // Null-terminate
    if (current_idx < output_size)
        output[current_idx] = '\0';
    else
        output[output_size - 1] = '\0';

    return nbr_lines;
}

// Turn off canonical mode + echo
static void enable_raw_mode(struct termios *old) {
    struct termios new;
    tcgetattr(STDIN_FILENO, old); // get current settings
    new = *old;
    new.c_lflag &= ~(ICANON | ECHO); // disable canonical mode & echo
    tcsetattr(STDIN_FILENO, TCSANOW, &new);
}

// Restore original terminal settings
static void disable_raw_mode(struct termios *old) {
    tcsetattr(STDIN_FILENO, TCSANOW, old);
}

static void handle_signal(int sig) {
    (void)sig;              // Sig is not used
    disable_raw_mode(&old); // restore terminal settings
    printf("\033[?25h\033[0 q\n");  // show cursor again + restore to block
    printf("Caught signal, exiting...\n");
    exit(1);
}

static void print_text(const char *text, int *correct_chars, int current_idx,
                       int current_line) {
    int len = strlen(text);

    printf("\033[2K\r"); // Clear line

    for (int i = 0; i < current_line - 1; i++) {
        printf("\033[1A");   // move cursor up
        printf("\033[2K\r"); // Clear line
    }

    for (int i = 0; i < len; i++) {
        if (i < current_idx) {
            if (!correct_chars[i]) {
                // Red for failed char, underscore if it was a space
                if (text[i] == ' ')
                    printf("\033[31m_\033[0m");
                else
                    printf("\033[31m%c\033[0m", text[i]);
            } else {
                printf("\033[32m%c\033[0m", text[i]); // Green for correct
            }
        } else {
            printf("%c", text[i]); // Not yet typed
        }
    }

    fflush(stdout);
}

int main(int argc, char *argv[]) {
    args args;
    if (parse_arguments(argc, argv, &args) != 0)
        return 1;

    // Catch termination signals and exit gracefully
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Seed the random generator
    srand(time(NULL));

    char **words = NULL;
    int word_count = read_words(args.words_file, &words);
    if (word_count < 0) {
        return 1;
    }

    char text[1000];

    int nbr_lines = build_test_text(words, word_count, text, sizeof(text),
                                    args.num_words, get_terminal_width());
    int current_line = 1;
    int current_idx = 0;
    int col = 0;

    int text_len = strlen(text);

    // Save terminal mode
    enable_raw_mode(&old);

    // Keep track of correct keystrokes for text
    int *correct_keystrokes_list = calloc(text_len, sizeof(int));
    if (!correct_keystrokes_list) {
        perror("calloc failed");
        return 1;
    }
    for (int i = 0; i < text_len; i++) {
        correct_keystrokes_list[i] = 1; // initialize with 1
    }

    // Init stats for this game
    stats game_stats;
    init_stats(&game_stats);

    // Count down
    printf("\033[?25l"); // hide cursor
    for (int i = 3; i > 0; i--) {
        printf("Game starts in: %d\n\033[90m%s\033[0m", i,
               text); // print the text in gray
        fflush(stdout);
        usleep(1000000);
        for (int i = 0; i < nbr_lines; i++) {
            printf("\033[2K\r"); // clear text line
            printf("\033[1A");   // move cursor up
        }
        printf("\033[2K\r"); // clear counter line
    }
    printf("\033[?25h"); // show cursor again
    printf("\033[6 q"); // bar cursor

    // Initial display
    printf("GO!\n%s", text);
    tcflush(STDIN_FILENO, TCIFLUSH); // Clear pending input

    // Start timer
    struct timeval start, end, key_timer_start, key_timer_end;
    gettimeofday(&start, NULL);
    gettimeofday(&key_timer_start, NULL);

    while (current_idx < text_len) {
        // Continue on line break
        if (text[current_idx] == '\n') {
            current_line++;
            col = 0; // for cursor position
            current_idx++;
            continue;
        }

        // Move cursor to current line
        for (int i = 0; i < nbr_lines - current_line; i++) {
            printf("\033[1A");
        }
        // Move cursor to current col
        printf("\033[%dG", col + 1);

        // Read input
        char input;
        scanf("%c", &input);

        if (input == text[current_idx]) {
            // Stop and start new key timer
            gettimeofday(&key_timer_end, NULL);
            double elapsed_sec_for_key =
                (key_timer_end.tv_sec - key_timer_start.tv_sec) +
                (key_timer_end.tv_usec - key_timer_start.tv_usec) / 1e6;
            gettimeofday(&key_timer_start, NULL);

            // Add success or fail for key
            char prev_key = '\0'; // default
            if (current_idx > 0) {
                prev_key = text[current_idx - 1];
            }
            update_key_stats(&game_stats, input,
                             correct_keystrokes_list[current_idx],
                             elapsed_sec_for_key, prev_key);

            current_idx++;
            col++;
        } else {
            correct_keystrokes_list[current_idx] = 0;
        }

        print_text(text, correct_keystrokes_list, current_idx, current_line);
    }

    // Stop timer
    gettimeofday(&end, NULL);
    double game_elapsed_sec =
        (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    // Calculate wpm
    double game_wpm = calc_wpm(text_len, game_elapsed_sec);

    // Sum correct keystrokes
    int correct_keystrokes = 0;
    for (int i = 0; i < text_len; i++) {
        if (correct_keystrokes_list[i] == 1) {
            correct_keystrokes++;
        }
    }

    disable_raw_mode(&old);
    printf("\033[0 q"); // restore block cursor
    free(correct_keystrokes_list);

    double game_acc = calc_acc(text_len, correct_keystrokes);

    printf("\nDone!\n");

    update_total_stats(&game_stats, text_len, correct_keystrokes,
                       game_elapsed_sec, game_wpm);

    // Load stats for player
    stats player_stats;
    if (!load_stats(args.player_name, &player_stats)) {
        init_stats(&player_stats);
    }

    // Add game stats to the player
    merge_stats(&player_stats, &game_stats);

    save_game_history(args.player_name, &game_stats);
    save_stats(args.player_name, &player_stats);

    double avg_wpm = calc_wpm(player_stats.total.total_keystrokes,
                              player_stats.total.time_spent);
    double wpm_diff = game_wpm - avg_wpm;

    double avg_acc = calc_acc(player_stats.total.total_keystrokes,
                              player_stats.total.correct_keystrokes);
    double acc_diff = game_acc - avg_acc;

    if (wpm_diff < 0) {
        printf("Speed: %.1fwpm (\033[31m↓%.1fwpm\033[0m)\n", game_wpm,
               wpm_diff);
    } else {
        printf("Speed: %.1fwpm (\033[32m↑+%.1fwpm\033[0m)\n", game_wpm,
               wpm_diff);
    }
    if (acc_diff < 0) {
        printf("Accuracy: %.2f%% (\033[31m↓%.2f%%\033[0m)\n", game_acc,
               acc_diff);
    } else {
        printf("Accuracy: %.2f%% (\033[32m↑+%.2f%%\033[0m)\n", game_acc,
               acc_diff);
    }

    print_stats(&player_stats);
}
