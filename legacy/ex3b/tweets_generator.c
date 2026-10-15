#include "markov_chain.h"
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

// Don't change the macros!
#define FILE_PATH_ERROR "Error: incorrect file path"
#define NUM_ARGS_ERROR "Usage: invalid number of arguments"
#define DELIMITERS " \t\r\n"
#define MAX_LINE_LENGTH 1000
#define MAX_TWEET_LENGTH 20

static int comp_strings(void *data1, void *data2);
static void *copy_string(void *data);
static void free_string(void *data);
static bool is_last_word(void *data);
static void print_string(void *data);
int fill_database(FILE *fp, int words_to_read, MarkovChain *markov_chain);
long parse_long(const char *str, const char *arg_name);

int main(int argc, char *argv[])
{
    if (argc != 4 && argc != 5)
    {
        printf(NUM_ARGS_ERROR);
        return EXIT_FAILURE;
    }

    // Parse Seed
    long seed_val = parse_long(argv[1], "seed");
    if (seed_val == -1)
    {
        return EXIT_FAILURE;
    }
    unsigned int seed = (unsigned int)seed_val;

    // Parse Num Tweets
    long tweets_val = parse_long(argv[2], "num_tweets");
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
        long words_val = parse_long(argv[4], "words_to_read");
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

int fill_database(FILE *fp, int words_to_read, MarkovChain *markov_chain)
{
    char line[MAX_LINE_LENGTH];
    int words_read = 0;
    Node *prev_node_wrapper = NULL;

    while (fgets(line, MAX_LINE_LENGTH, fp))
    {
        char *token = strtok(line, DELIMITERS);
        while (token)
        {
            if (words_to_read != -1 && words_read >= words_to_read)
            {
                return 0;
            }

            Node *curr_node_wrapper = add_to_database(markov_chain, token);
            if (!curr_node_wrapper)
            {
                printf(ALLOCATION_ERROR_MESSAGE);
                return 1;
            }
            words_read++;
            
            MarkovNode *curr_mnode = curr_node_wrapper->data;
            MarkovNode *prev_mnode = prev_node_wrapper ? prev_node_wrapper->data : NULL;

            if (prev_mnode)
            {
                if (add_node_to_frequency_list(prev_mnode, curr_mnode) != 0) {
                     printf(ALLOCATION_ERROR_MESSAGE);
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
    return 0;
}

long parse_long(const char *str, const char *arg_name)
{
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if (errno != 0 || *endptr != '\0' || val < 0)
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