#include "markov_chain.h"
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

// Don't change the macros!
#define FILE_PATH_ERROR "Error: incorrect file path"
#define NUM_ARGS_ERROR "Usage: invalid number of arguments"
#define DELIMITERS " \t\r\n"
#define LINE_BUFFER_START 1024
#define MAX_TWEET_LENGTH 20

static int comp_strings(void *data1, void *data2);
static void *copy_string(void *data);
static void free_string(void *data);
static bool is_last_word(void *data);
static void print_string(void *data);
int fill_database(FILE *fp, int words_to_read, MarkovChain *markov_chain);
long parse_long(const char *str, const char *arg_name, unsigned long max);

int main(int argc, char *argv[])
{
    if (argc != 4 && argc != 5)
    {
        printf(NUM_ARGS_ERROR);
        return EXIT_FAILURE;
    }

    // Parse Seed
    long seed_val = parse_long(argv[1], "seed", UINT_MAX);
    if (seed_val == -1)
    {
        return EXIT_FAILURE;
    }
    unsigned int seed = (unsigned int)seed_val;

    // Parse Num Tweets
    long tweets_val = parse_long(argv[2], "num_tweets", (unsigned long) INT_MAX);
    if (tweets_val == -1)
    {
        return EXIT_FAILURE;
    }
    int num_tweets = (int)tweets_val;

    // Parse Words to Read
    char *file_path = argv[3];
    int words_to_read = -1;
    if (argc == 5)
    {
        long words_val = parse_long(argv[4], "words_to_read", (unsigned long) INT_MAX);
        if (words_val == -1) 
        {
            return EXIT_FAILURE;
        }
        words_to_read = (int)words_val;
    }

    srand(seed);

    FILE *fp = fopen(file_path, "r");
    if (!fp)
    {
        printf(FILE_PATH_ERROR);
        return EXIT_FAILURE;
    }

    MarkovChain *markov_chain = malloc(sizeof(MarkovChain));
    if (!markov_chain)
    {
        printf(ALLOCATION_ERROR_MESSAGE);
        fclose(fp);
        return EXIT_FAILURE;
    }

    markov_chain->database = malloc(sizeof(LinkedList));
    if (!markov_chain->database)
    {
        printf(ALLOCATION_ERROR_MESSAGE);
        free(markov_chain);
        fclose(fp);
        return EXIT_FAILURE;
    }

    markov_chain->database->first = NULL;
    markov_chain->database->last = NULL;
    markov_chain->database->size = 0;
    markov_chain->print_func = print_string;
    markov_chain->comp_func = comp_strings;
    markov_chain->free_data = free_string;
    markov_chain->copy_func = copy_string;
    markov_chain->is_last = is_last_word;

    if (fill_database(fp, words_to_read, markov_chain) != 0) 
    {
        free_database(&markov_chain);
        fclose(fp);
        return EXIT_FAILURE;
    }
    fclose(fp);

    for (int i = 0; i < num_tweets; i++)
    {
        MarkovNode *start_node = get_first_random_node(markov_chain);
        if (start_node) 
        {
            printf("Tweet %d: ", i + 1);
            generate_random_sequence(markov_chain, start_node, MAX_TWEET_LENGTH);
        }
    }

    free_database(&markov_chain);
    return EXIT_SUCCESS;
}

/**
 * Read one whole line, of any length, into *line (growing it as needed).
 * Replaces a fixed fgets() buffer, which silently split long words in two.
 * @return 1 if a line was read, 0 at end of file, -1 on allocation failure
 */
static int read_line(FILE *fp, char **line, size_t *cap)
{
    size_t len = 0;
    int c;
    while ((c = fgetc(fp)) != EOF)
    {
        if (len + 1 >= *cap)
        {
            size_t new_cap = *cap ? *cap * 2 : LINE_BUFFER_START;
            char *tmp = realloc(*line, new_cap);
            if (!tmp)
            {
                return -1;
            }
            *line = tmp;
            *cap = new_cap;
        }
        (*line)[len++] = (char) c;
        if (c == '\n')
        {
            break;
        }
    }
    if (len == 0)
    {
        return 0;
    }
    (*line)[len] = '\0';
    return 1;
}

int fill_database(FILE *fp, int words_to_read, MarkovChain *markov_chain)
{
    char *line = NULL;
    size_t line_cap = 0;
    int read_status;
    int words_read = 0;
    Node *prev_node_wrapper = NULL;

    while ((read_status = read_line(fp, &line, &line_cap)) == 1)
    {
        char *token = strtok(line, DELIMITERS);
        while (token)
        {
            if (words_to_read != -1 && words_read >= words_to_read)
            {
                free(line);
                return 0;
            }

            Node *curr_node_wrapper = add_to_database(markov_chain, token);
            if (!curr_node_wrapper)
            {
                printf(ALLOCATION_ERROR_MESSAGE);
                free(line);
                return 1;
            }
            words_read++;
            
            MarkovNode *curr_mnode = curr_node_wrapper->data;
            MarkovNode *prev_mnode = prev_node_wrapper ? prev_node_wrapper->data : NULL;

            if (prev_mnode)
            {
                if (add_node_to_frequency_list(prev_mnode, curr_mnode) != 0) {
                     printf(ALLOCATION_ERROR_MESSAGE);
                     free(line);
                     return 1;
                }
            }

            if (is_last_word(token))
            {
                prev_node_wrapper = NULL;
            }
            else
            {
                prev_node_wrapper = curr_node_wrapper;
            }

            token = strtok(NULL, DELIMITERS);
        }
    }
    free(line);
    if (read_status < 0)
    {
        printf(ALLOCATION_ERROR_MESSAGE);
        return 1;
    }
    return 0;
}

long parse_long(const char *str, const char *arg_name, unsigned long max)
{
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if (errno != 0 || endptr == str || *endptr != '\0' || val < 0
        || (unsigned long) val > max)
    {
        fprintf(stderr, "Error: Invalid value for %s\n", arg_name);
        return -1;
    }
    return val;
}

static void print_string(void *data)
{
    const char *str = (const char *)data;
    size_t len = strlen(str);

    if (len > 0 && str[len-1] == '.')
    {
        printf("%s", str);
    } else {
        printf("%s ", str);
    }
}

static int comp_strings(void *data1, void *data2)
{
    return strcmp((char *)data1, (char *)data2);
}

static void free_string(void *data)
{
    free(data);
}

static void *copy_string(void *data)
{
    char *src = (char *)data;
    char *dest = malloc(strlen(src) + 1);
    if (dest != NULL)
    {
        strcpy(dest, src);
    }
    return dest;
}

static bool is_last_word(void *data)
{
    char *str = (char *)data;
    size_t len = strlen(str);
    return (len > 0 && str[len-1] == '.');
}
