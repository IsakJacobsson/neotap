#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_words.h"

#define MAX_WORD_LEN 100 // maximum length of each word

int read_words(const char *filename, char ***words_out) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Could not open file");
        return -1;
    }

    char buffer[MAX_WORD_LEN];
    int count = 0;
    int capacity = 10; // initial size
    char **words = malloc(capacity * sizeof(char *));
    if (!words) {
        perror("malloc failed");
        fclose(fp);
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), fp)) {
        buffer[strcspn(buffer, "\n")] = '\0'; // remove newline

        if (count >= capacity) {
            capacity *= 2;
            char **tmp = realloc(words, capacity * sizeof(char *));
            if (!tmp) {
                perror("realloc failed");
                // free already allocated strings
                for (int i = 0; i < count; i++)
                    free(words[i]);
                free(words);
                fclose(fp);
                return -1;
            }
            words = tmp;
        }

        words[count] = strdup(buffer); // allocate + copy
        if (!words[count]) {
            perror("strdup failed");
            // cleanup
            for (int i = 0; i < count; i++)
                free(words[i]);
            free(words);
            fclose(fp);
            return -1;
        }
        count++;
    }

    fclose(fp);
    *words_out = words; // hand back allocated array
    return count;
}