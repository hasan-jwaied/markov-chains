#ifndef _MARKOV_CHAIN_H_
#define _MARKOV_CHAIN_H_

#include "linked_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#define ALLOCATION_ERROR_MESSAGE "Allocation failure: Failed to allocate new memory\n"

typedef struct MarkovChain
{
    LinkedList *database;
} MarkovChain;

typedef struct MarkovNode
{
    char *data;
    struct MarkovNodeFrequency *frequency_list;
    size_t frequency_list_len;
} MarkovNode;

typedef struct MarkovNodeFrequency
{
    struct MarkovNode *markov_node;
    int frequency;
} MarkovNodeFrequency;

/**
* Check if data_ptr is in database. If so, return the Node wrapping it in
* the markov_chain, otherwise return NULL.
*/
Node* get_node_from_database(MarkovChain *markov_chain, char *data_ptr);

/**
* If data_ptr in markov_chain, return it's node. Otherwise, create new
 * node, add to end of markov_chain's database and return it.
 */
Node* add_to_database(MarkovChain *markov_chain, char *data_ptr);

/**
 * Add the second markov_node to the frequency list of the first markov_node.
 * If already in list, update its occurrence frequency value.
 */
int add_node_to_frequency_list(MarkovNode *first_node, MarkovNode *second_node);

/**
 * Free markov_chain and all of it's content from memory
 */
void free_database(MarkovChain **ptr_chain);

/**
 * Get one random MarkovNode from the given markov_chain's database.
 */
MarkovNode* get_first_random_node(const MarkovChain *markov_chain);

/**
 * Choose randomly the next MarkovNode, depend on its occurrence frequency.
 */
MarkovNode* get_next_random_node(const MarkovNode *cur_markov_node);

/**
 * Receive markov_chain, generate and print random sentence out of it.
 */
void generate_tweet(MarkovNode *first_node, int max_length);

#endif /* _MARKOV_CHAIN_H_ */
