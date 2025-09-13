#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>

#include "parse_words.h"
#include "stats.h"

#define NBR_TEXT_WORDS 10 // words in text

void build_test_text(
    char **words,
    size_t num_words,
    char *output,
    size_t output_size,
    size_t num_test_words)
{
    if (!words || !output || output_size == 0 || num_words == 0)
        return;

    size_t current_idx = 0;

    for (size_t i = 0; i < num_test_words; i++)
    {
        int word_idx = rand() % num_words;
        char *word = words[word_idx];
        size_t word_len = strlen(word);

        // Check if the word fits in the remaining buffer (including space or null)
        size_t space_needed = word_len;
        if (i > 0)
            space_needed += 1; // space before word if not first

        if (current_idx + space_needed >= output_size)
        {
            break; // not enough space
        }

        // Add space if not the first word
        if (i > 0)
        {
            output[current_idx++] = ' ';
        }

        // Copy the word
        memcpy(&output[current_idx], word, word_len);
        current_idx += word_len;
    }

    // Null-terminate
    output[current_idx] = '\0';
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

void print_text(const char *text, int *correct_chars, int current_idx)
{
    int len = strlen(text);

    printf("\033[2K\r"); // Clear the current line

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
    if (argc < 2)
    {
        printf("Usage: %s <player_name>\n", argv[0]);
        return 1;
    }
    const char *player_name = argv[1];

    // Seed the random generator
    srand(time(NULL));

    int current_idx = 0;

    char **words = NULL;
    int word_count = read_words("words.txt", &words);
    if (word_count < 0)
    {
        return 1;
    }

    char text[1000];

    build_test_text(words, word_count, text, sizeof(text), NBR_TEXT_WORDS);

    int text_len = strlen(text);

    // Save terminal mode
    struct termios old;
    enable_raw_mode(&old);

    // Keep track of the failed chars
    int *correct_chars = calloc(text_len, sizeof(int));
    if (!correct_chars)
    {
        perror("calloc failed");
        return 1;
    }
    for (int i = 0; i < text_len; i++)
    {
        correct_chars[i] = 1; // initialize with 1
    }

    stats stats;

    // Load or initialize stats
    if (!load_stats(player_name, &stats))
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
        printf("\033[2K\r"); // clear text line
        printf("\033[1A");   // move cursor up
        printf("\033[2K\r"); // clear counter line
    }
    printf("\033[?25h"); // show cursor again

    // Initial display
    printf("GO!\n%s", text);

    // Start timer
    struct timeval start, end, key_timer_start, key_timer_end;
    gettimeofday(&start, NULL);
    gettimeofday(&key_timer_start, NULL);

    while (current_idx < text_len)
    {
        // Move cursor to current position
        printf("\033[%dG", current_idx + 1);

        // Read input
        tcflush(STDIN_FILENO, TCIFLUSH); // Clear pending input
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
            update_key_stats(&stats, input, correct_chars[current_idx], elapsed_sec_for_key);

            current_idx++;
        }
        else
        {
            correct_chars[current_idx] = 0;
        }

        print_text(text, correct_chars, current_idx);
    }

    // Stop timer
    gettimeofday(&end, NULL);
    double game_elapsed_sec = (end.tv_sec - start.tv_sec) +
                              (end.tv_usec - start.tv_usec) / 1e6;

    // Calculate wpm
    double game_wpm = calc_wpm(text_len, game_elapsed_sec);

    // Tally successes and fails
    int nbr_successes = 0;
    for (int i = 0; i < text_len; i++)
    {
        if (correct_chars[i] == 1)
        {
            nbr_successes++;
        }
    }

    disable_raw_mode(&old);
    free(correct_chars);

    double game_acc = calc_acc(text_len, nbr_successes);

    if (game_wpm > stats.total.best_wpm)
    {
        printf("\nDone! New record!\n");
    }
    else
    {
        printf("\nDone!\n");
    }

    update_total_stats(&stats, text_len, nbr_successes, game_elapsed_sec, game_wpm);
    save_stats(player_name, &stats);

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
