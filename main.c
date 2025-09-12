#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>

#define NBR_WORDS 1000    // words to read
#define MAX_WORD_LEN 100  // maximum length of each word
#define NBR_TEXT_WORDS 10 // words in text
#define NUM_KEYS 25       // a to y
#define KEY_STATS_FILE_BASE_NAME "stats/key_stats"
#define OVERALL_STATS_FILE_BASE_NAME "stats/overall_stats"

typedef struct
{
    char key;
    int successes;
    int fails;
    double total_time;
    int attempts;
} key_stat;

typedef struct
{
    int successes;
    int fails;
    double total_time;
} overall_stats;

void load_key_stats(const char *filename, key_stat key_stats[])
{
    // initialize defaults first
    for (int i = 0; i < NUM_KEYS; i++)
    {
        key_stats[i].key = 'a' + i;
        key_stats[i].successes = 0;
        key_stats[i].fails = 0;
        key_stats[i].total_time = 0.0;
        key_stats[i].attempts = 0;
    }

    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        // no file → keep defaults
        return;
    }

    char key;
    int successes, fails;
    double total_time;

    while (fscanf(fp, " %c %d %d %lf", &key, &successes, &fails, &total_time) == 4)
    {
        int idx = key - 'a';
        key_stats[idx].key = key;
        key_stats[idx].successes = successes;
        key_stats[idx].fails = fails;
        key_stats[idx].total_time = total_time;
    }

    fclose(fp);
}

void save_key_stats(const char *filename, key_stat key_stats[])
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        perror("Could not open file");
        exit(1);
    }
    for (int i = 0; i < NUM_KEYS; i++)
    {
        fprintf(fp, "%c %d %d %lf\n", key_stats[i].key, key_stats[i].successes, key_stats[i].fails, key_stats[i].total_time);
    }
    fclose(fp);
}

void load_overall_stats(const char *filename, overall_stats *stats)
{
    // defaults
    stats->successes = 0;
    stats->fails = 0;
    stats->total_time = 0.0;

    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        // no file → keep defaults
        return;
    }

    int successes, fails;
    double total_time;

    if (fscanf(fp, " %d %d %lf", &successes, &fails, &total_time) == 3)
    {
        stats->successes = successes;
        stats->fails = fails;
        stats->total_time = total_time;
    }

    fclose(fp);
}

void save_overall_stats(const char *filename, overall_stats *stats)
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        perror("Could not open file");
        exit(1);
    }

    fprintf(fp, "%d %d %lf\n", stats->successes, stats->fails, stats->total_time);

    fclose(fp);
}

int read_words(const char *filename, char *words[], int max_words)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror("Could not open file");
        return -1;
    }

    char buffer[MAX_WORD_LEN];
    int count = 0;

    while (fgets(buffer, sizeof(buffer), fp) && count < max_words)
    {
        buffer[strcspn(buffer, "\n")] = '\0'; // remove newline
        words[count] = strdup(buffer);        // allocate and copy
        if (!words[count])
        {
            perror("Memory allocation failed");
            break;
        }
        count++;
    }

    fclose(fp);
    return count;
}

double average_wpm(int successes, int fails, double total_time)
{
    int total_chars = successes + fails;
    if (total_time <= 0.0)
    {
        return 0.0; // avoid division by zero
    }

    double words_typed = (double)total_chars / 5.0; // convert chars to "words"

    return words_typed / total_time * 60.0;
}

void build_test_text(
    char *words[],
    size_t num_words,
    char *output,
    size_t output_size,
    size_t num_test_words)
{
    if (output_size == 0)
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

void print_text(const char *text, int *failed_chars, int current_idx)
{
    int len = strlen(text);

    printf("\033[2K\r"); // Clear the current line

    for (int i = 0; i < len; i++)
    {
        if (i < current_idx)
        {
            if (failed_chars[i])
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

    char *words[NBR_WORDS];

    int word_count = read_words("words.txt", words, NBR_WORDS);
    if (word_count < 0)
    {
        return 1;
    }

    char text[1000];

    build_test_text(words, NBR_WORDS, text, sizeof(text), NBR_TEXT_WORDS);

    int text_len = strlen(text);

    // Save terminal mode
    struct termios old;
    enable_raw_mode(&old);

    // Keep track of the failed chars
    int *failed_chars = calloc(text_len, sizeof(int));
    if (!failed_chars)
    {
        perror("calloc failed");
        return 1;
    }

    // Read stats
    key_stat key_stats[NUM_KEYS];
    overall_stats stats;
    char key_stats_file[256];
    snprintf(key_stats_file, sizeof(key_stats_file), "%s_%s.txt",
             KEY_STATS_FILE_BASE_NAME, player_name);
    char overall_stats_file[256];
    snprintf(overall_stats_file, sizeof(key_stats_file), "%s_%s.txt",
             OVERALL_STATS_FILE_BASE_NAME, player_name);
    load_key_stats(key_stats_file, key_stats);
    load_overall_stats(overall_stats_file, &stats);

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
            gettimeofday(&key_timer_end, NULL);
            double elapsed_sec_for_key = (key_timer_end.tv_sec - key_timer_start.tv_sec) +
                                         (key_timer_end.tv_usec - key_timer_start.tv_usec) / 1e6;

            // Add success or fail for key
            if (input != ' ') // We don't keep track of space key stats
            {
                if (failed_chars[current_idx] == 0)
                {
                    key_stats[input - 'a'].successes++;
                }
                else
                {
                    key_stats[input - 'a'].fails++;
                }
                key_stats[input - 'a'].total_time += elapsed_sec_for_key;
            }
            current_idx++;

            // Start timer for next key
            gettimeofday(&key_timer_start, NULL);
        }
        else
        {
            failed_chars[current_idx] = 1;
        }

        print_text(text, failed_chars, current_idx);
    }

    // Stop timer
    gettimeofday(&end, NULL);
    double elapsed_sec = (end.tv_sec - start.tv_sec) +
                         (end.tv_usec - start.tv_usec) / 1e6;

    // Calculate wpm
    double nbr_words = (double)text_len / 5.0;
    double words_per_minute = nbr_words / elapsed_sec * 60.0;

    // Tally successes and fails
    int nbr_successes = 0;
    int nbr_fails = 0;
    for (int i = 0; i < text_len; i++)
    {
        if (failed_chars[i] == 1)
        {
            nbr_fails++;
        }
        else
        {
            nbr_successes++;
        }
    }

    // Save stats
    stats.successes += nbr_successes;
    stats.fails += nbr_fails;
    stats.total_time += elapsed_sec;
    save_overall_stats(overall_stats_file, &stats);
    save_key_stats(key_stats_file, key_stats);

    double accuracy = (double)(nbr_successes) / (double)text_len * 100.0;

    disable_raw_mode(&old);
    free(failed_chars);

    double avg_wpm = average_wpm(stats.successes, stats.fails, stats.total_time);
    double wpm_diff = words_per_minute - avg_wpm;

    double avg_acc = (double)stats.successes / (double)(stats.successes + stats.fails) * 100.0;
    double acc_diff = accuracy - avg_acc;

    printf("\nDone!\n");

    if (wpm_diff < 0)
    {
        printf("Speed: %.1fwpm (\033[31m↓%.1fwpm\033[0m)\n", words_per_minute, wpm_diff);
    }
    else
    {
        printf("Speed: %.1fwpm (\033[32m↑+%.1fwpm\033[0m)\n", words_per_minute, wpm_diff);
    }
    if (acc_diff < 0)
    {
        printf("Accuracy: %.2f%% (\033[31m↓%.2f%%\033[0m)\n", accuracy, acc_diff);
    }
    else
    {
        printf("Accuracy: %.2f%% (\033[32m↑+%.2f%%\033[0m)\n", accuracy, acc_diff);
    }
}
