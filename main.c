#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include "parse_args.h"
#include "parse_words.h"
#include "stats.h"

int get_terminal_width(void)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
        return 80; // fallback
    return w.ws_col;
}

int build_test_text(
    char **words,
    size_t num_words,
    char *output,
    size_t output_size,
    size_t num_test_words,
    int term_width)
{
    if (!words || !output || output_size == 0 || num_words == 0)
        return 0;

    size_t current_idx = 0;
    int col = 0;       // current column in the terminal
    int nbr_lines = 1; // start with first line

    for (size_t i = 0; i < num_test_words; i++)
    {
        int word_idx = rand() % num_words;
        char *word = words[word_idx];
        size_t word_len = strlen(word);

        // Truncate word if it's too long for terminal
        if (word_len >= term_width)
            word_len = term_width - 1;

        // Add space if not first word
        if (i > 0)
        {
            // Wrap to new line if space + word exceeds terminal width
            if (col + 1 + word_len >= term_width)
            {
                // Ensure we have space for newline
                if (current_idx + 1 >= output_size)
                    break;

                // Add a space at the end of the line
                if (col < term_width && current_idx < output_size)
                {
                    output[current_idx++] = ' ';
                }

                output[current_idx++] = '\n';
                col = 0;
                nbr_lines++;
            }
            else
            {
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
        if (col >= term_width)
        {
            if (current_idx + 1 < output_size)
            {
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
void enable_raw_mode(struct termios *old)
{
    struct termios new;
    tcgetattr(STDIN_FILENO, old); // get current settings
    new = *old;
    new.c_lflag &= ~(ICANON | ECHO); // disable canonical mode & echo
    tcsetattr(STDIN_FILENO, TCSANOW, &new);
}

// Restore original terminal settings
void disable_raw_mode(struct termios *old)
{
    tcsetattr(STDIN_FILENO, TCSANOW, old);
}

void print_text(const char *text, int *correct_chars, int current_idx, int current_line)
{
    int len = strlen(text);

    printf("\033[2K\r"); // Clear line

    for (int i = 0; i < current_line - 1; i++)
    {
        printf("\033[1A");   // move cursor up
        printf("\033[2K\r"); // Clear line
    }

    for (int i = 0; i < len; i++)
    {
        if (i < current_idx)
        {
            if (!correct_chars[i])
            {
                // Red for failed char, underscore if it was a space
                if (text[i] == ' ')
                    printf("\033[31m_\033[0m");
                else
                    printf("\033[31m%c\033[0m", text[i]);
            }
            else
            {
                printf("\033[32m%c\033[0m", text[i]); // Green for correct
            }
        }
        else
        {
            printf("%c", text[i]); // Not yet typed
        }
    }

    fflush(stdout);
}

int main(int argc, char *argv[])
{
    args args;
    if (parse_arguments(argc, argv, &args) != 0)
        return 1;

    // Seed the random generator
    srand(time(NULL));

    char **words = NULL;
    int word_count = read_words("words.txt", &words);
    if (word_count < 0)
    {
        return 1;
    }

    char text[1000];

    int nbr_lines = build_test_text(words, word_count, text, sizeof(text), args.num_words, get_terminal_width());
    int current_line = 1;
    int current_idx = 0;
    int col = 0;

    int text_len = strlen(text);

    // Save terminal mode
    struct termios old;
    enable_raw_mode(&old);

    // Keep track of correct keystrokes for text
    int *correct_keystrokes_list = calloc(text_len, sizeof(int));
    if (!correct_keystrokes_list)
    {
        perror("calloc failed");
        return 1;
    }
    for (int i = 0; i < text_len; i++)
    {
        correct_keystrokes_list[i] = 1; // initialize with 1
    }

    stats stats;

    // Load or initialize stats
    if (!load_stats(args.player_name, &stats))
    {
        init_stats(&stats);
    }

    // Count down
    printf("\033[?25l"); // hide cursor
    for (int i = 3; i > 0; i--)
    {
        printf("Game starts in: %d\n\033[90m%s\033[0m", i, text); // print the text in gray
        fflush(stdout);
        usleep(1000000);
        for (int i = 0; i < nbr_lines; i++)
        {
            printf("\033[2K\r"); // clear text line
            printf("\033[1A");   // move cursor up
        }
        printf("\033[2K\r"); // clear counter line
    }
    printf("\033[?25h"); // show cursor again

    // Initial display
    printf("GO!\n%s", text);
    tcflush(STDIN_FILENO, TCIFLUSH); // Clear pending input

    // Start timer
    struct timeval start, end, key_timer_start, key_timer_end;
    gettimeofday(&start, NULL);
    gettimeofday(&key_timer_start, NULL);

    while (current_idx < text_len)
    {
        // Continue on line break
        if (text[current_idx] == '\n')
        {
            current_line++;
            col = 0; // for cursor position
            current_idx++;
            continue;
        }

        // Move cursor to current line
        for (int i = 0; i < nbr_lines - current_line; i++)
        {
            printf("\033[1A");
        }
        // Move cursor to current col
        printf("\033[%dG", col + 1);

        // Read input
        char input;
        scanf("%c", &input);

        if (input == text[current_idx])
        {
            // Stop and start new key timer
            gettimeofday(&key_timer_end, NULL);
            double elapsed_sec_for_key = (key_timer_end.tv_sec - key_timer_start.tv_sec) +
                                         (key_timer_end.tv_usec - key_timer_start.tv_usec) / 1e6;
            gettimeofday(&key_timer_start, NULL);

            // Add success or fail for key
            update_key_stats(&stats, input, correct_keystrokes_list[current_idx], elapsed_sec_for_key);

            current_idx++;
            col++;
        }
        else
        {
            correct_keystrokes_list[current_idx] = 0;
        }

        print_text(text, correct_keystrokes_list, current_idx, current_line);
    }

    // Stop timer
    gettimeofday(&end, NULL);
    double game_elapsed_sec = (end.tv_sec - start.tv_sec) +
                              (end.tv_usec - start.tv_usec) / 1e6;

    // Calculate wpm
    double game_wpm = calc_wpm(text_len, game_elapsed_sec);

    // Sum correct keystrokes
    int correct_keystrokes = 0;
    for (int i = 0; i < text_len; i++)
    {
        if (correct_keystrokes_list[i] == 1)
        {
            correct_keystrokes++;
        }
    }

    disable_raw_mode(&old);
    free(correct_keystrokes_list);

    double game_acc = calc_acc(text_len, correct_keystrokes);

    if (game_wpm > stats.total.best_wpm)
    {
        printf("\nDone! New record!\n");
    }
    else
    {
        printf("\nDone!\n");
    }

    update_total_stats(&stats, text_len, correct_keystrokes, game_elapsed_sec, game_wpm);
    save_stats(args.player_name, &stats);

    double avg_wpm = calc_wpm(stats.total.total_keystrokes, stats.total.time_spent);
    double wpm_diff = game_wpm - avg_wpm;

    double avg_acc = calc_acc(stats.total.total_keystrokes, stats.total.correct_keystrokes);
    double acc_diff = game_acc - avg_acc;

    if (wpm_diff < 0)
    {
        printf("Speed: %.1fwpm (\033[31m↓%.1fwpm\033[0m)\n", game_wpm, wpm_diff);
    }
    else
    {
        printf("Speed: %.1fwpm (\033[32m↑+%.1fwpm\033[0m)\n", game_wpm, wpm_diff);
    }
    if (acc_diff < 0)
    {
        printf("Accuracy: %.2f%% (\033[31m↓%.2f%%\033[0m)\n", game_acc, acc_diff);
    }
    else
    {
        printf("Accuracy: %.2f%% (\033[32m↑+%.2f%%\033[0m)\n", game_acc, acc_diff);
    }

    print_stats(&stats);
}
