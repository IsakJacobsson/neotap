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
#define NUM_KEYS 26       // a-z
#define STATS_FILE_BASE_NAME "stats/"

typedef struct
{
    int pressed;
    int correct;
    double time_spent;
} key_stats;

typedef struct
{
    int games_played;
    int total_keystrokes;
    int correct_keystrokes;
    double time_spent;
    double best_wpm;
} total_stats;

typedef struct
{
    total_stats total;
    key_stats per_key[NUM_KEYS];
} stats;

void init_stats(stats *s)
{
    s->total.games_played = 0;
    s->total.total_keystrokes = 0;
    s->total.correct_keystrokes = 0;
    s->total.time_spent = 0.0;
    s->total.best_wpm = 0.0;

    for (int i = 0; i < NUM_KEYS; i++)
    {
        s->per_key[i].pressed = 0;
        s->per_key[i].correct = 0;
        s->per_key[i].time_spent = 0.0;
    }
}

void update_key_stats(stats *s, char key_char, int correct, double time_taken)
{
    if (key_char < 'a' || key_char > 'z')
        return;

    int index = key_char - 'a';
    s->per_key[index].pressed += 1;
    if (correct)
        s->per_key[index].correct += 1;
    s->per_key[index].time_spent += time_taken;
}

void update_total_stats(stats *stats, int total_keystrokes, int correct_keystrokes, double time, double wpm)
{
    stats->total.games_played++;
    stats->total.total_keystrokes += total_keystrokes;
    stats->total.correct_keystrokes += correct_keystrokes;
    stats->total.time_spent += time;

    if (wpm > stats->total.best_wpm)
    {
        stats->total.best_wpm = wpm;
    }
}

double get_key_wpm(key_stats *k)
{
    if (k->time_spent > 0)
        return (k->pressed / 5.0) / (k->time_spent / 60.0);
    return 0.0;
}

double get_key_accuracy(key_stats *k)
{
    if (k->pressed > 0)
        return ((double)k->correct / k->pressed) * 100.0;
    return 0.0;
}

void save_stats(const char *filename, stats *s)
{
    FILE *f = fopen(filename, "wb");
    if (f)
    {
        fwrite(s, sizeof(stats), 1, f);
        fclose(f);
    }
}

int loadStats(const char *filename, stats *s)
{
    FILE *f = fopen(filename, "rb");
    if (f)
    {
        fread(s, sizeof(stats), 1, f);
        fclose(f);
        return 1;
    }
    return 0;
}

double calc_wpm(int total_chars, double total_time)
{
    if (total_time <= 0.0)
    {
        return 0.0; // avoid division by zero
    }

    double words_typed = (double)total_chars / 5.0; // convert chars to "words"

    return words_typed / total_time * 60.0;
}

double calc_acc(int total_keystrokes, int correct_keystrokes)
{
    if (total_keystrokes == 0)
    {
        return 0.0; // avoid divide by zero
    }
    return ((double)correct_keystrokes / total_keystrokes) * 100.0;
}

void print_stats(const stats *s)
{
    // Print total stats
    double total_acc = calc_acc(s->total.total_keystrokes, s->total.correct_keystrokes);
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
    for (int i = 0; i < NUM_KEYS; i++)
    {
        const key_stats *k = &s->per_key[i];
        if (k->pressed > 0)
        { // only print used keys
            char key_char = 'a' + i;
            double acc = calc_acc(k->pressed, k->correct);
            double wpm = calc_wpm(k->pressed, k->time_spent);

            printf("Key '%c': accuracy=%.2f%%, WPM=%.2f\n", key_char, acc, wpm);
        }
    }
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

    char stats_file[256];
    snprintf(stats_file, sizeof(stats_file), "%s%s.dat",
             STATS_FILE_BASE_NAME, player_name);

    // Load or initialize stats
    if (!loadStats(stats_file, &stats))
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
            gettimeofday(&key_timer_end, NULL);
            double elapsed_sec_for_key = (key_timer_end.tv_sec - key_timer_start.tv_sec) +
                                         (key_timer_end.tv_usec - key_timer_start.tv_usec) / 1e6;

            // Add success or fail for key
            update_key_stats(&stats, input, correct_chars[current_idx], elapsed_sec_for_key);

            current_idx++;

            // Start timer for next key
            gettimeofday(&key_timer_start, NULL);
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
    save_stats(stats_file, &stats);

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
