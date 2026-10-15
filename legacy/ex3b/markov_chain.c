#include "markov_chain.h"
#include <string.h>
#include <stdbool.h>

/**
 * Get random number between 0 and max_number [0, max_number).
 */
int get_random_number(int max_number)
{
    return rand() % max_number;
}

Node* get_node_from_database(MarkovChain *markov_chain, void *data_ptr)
{
    if (!markov_chain || !data_ptr || !markov_chain->database)
    {
        return NULL;
    }

    Node *curr = markov_chain->database->first;
    while (curr)
    {
        MarkovNode *markov_node = (MarkovNode *) curr->data;
        if (markov_chain->comp_func(markov_node->data, data_ptr) == 0)
        {
            return curr;
        }
        curr = curr->next;
    }

    return NULL;
}

Node* add_to_database(MarkovChain *markov_chain, void *data_ptr)
{
    Node *existing_node = get_node_from_database(markov_chain, data_ptr);
    if (existing_node)
    {
        return existing_node;
    }

    MarkovNode *new_markov_node = malloc(sizeof(MarkovNode));
    if (!new_markov_node)
    {
        return NULL;
    }

    new_markov_node->data = markov_chain->copy_func(data_ptr);
    if (!new_markov_node->data)
    {
        free(new_markov_node);
        return NULL;
    }

    new_markov_node->frequency_list = NULL; 
    new_markov_node->frequencies_list_length = 0;

    if (add(markov_chain->database, new_markov_node) != 0)
    {
        markov_chain->free_data(new_markov_node->data);
        free(new_markov_node);
        return NULL;
    }
    
    return markov_chain->database->last;
}

int add_node_to_frequency_list(MarkovNode *first_node, MarkovNode *second_node)
{
    if (!first_node || !second_node) 
    {
        return 1;
    }

    // check if second_node already exists in the array
    for (size_t i = 0; i < first_node->frequencies_list_length; i++)
    {
        if (first_node->frequency_list[i].markov_node == second_node)
        {
            first_node->frequency_list[i].frequency++;
            return 0;
        }
    }

    // If not found, Reallocate memory for new size
    size_t new_size = first_node->frequencies_list_length + 1;
    MarkovNodeFrequency *temp = realloc(first_node->frequency_list, new_size * sizeof(MarkovNodeFrequency));
    if (!temp)
    {
        return 1;
    }

    first_node->frequency_list = temp;
    // Add the new node at the end of the array
    first_node->frequency_list[new_size - 1].markov_node = second_node;
    first_node->frequency_list[new_size - 1].frequency = 1;
    first_node->frequencies_list_length = new_size;

    return 0;
}

void free_database(MarkovChain **ptr_chain)
{
    if (ptr_chain == NULL || *ptr_chain == NULL)
    {
        return;
    }

    MarkovChain *chain = *ptr_chain;
    
    if (chain->database != NULL)
    {
        Node *current = chain->database->first;
        while (current != NULL)
        {
            Node *next = current->next;
            MarkovNode *mnode = (MarkovNode *)current->data;

            if (chain->free_data != NULL)
            {
                chain->free_data(mnode->data);
            }

            if (mnode->frequency_list != NULL)
            {
                free(mnode->frequency_list);
            }
            free(mnode);
            free(current);
            current = next;
        }
        free(chain->database);
    }
    free(chain);
    *ptr_chain = NULL;
}

MarkovNode* get_first_random_node(MarkovChain *markov_chain)
{
    if (!markov_chain || !markov_chain->database || markov_chain->database->size == 0)
    {
        return NULL;
    }

    // Loop until we find a word that DOES NOT end with a dot
    while (true)
    {
        int idx = get_random_number(markov_chain->database->size);
        Node *curr = markov_chain->database->first;
        for (int i = 0; i < idx; i++)
        {
            curr = curr->next;
        }
        
        MarkovNode *chosen_node = (MarkovNode *) curr->data;
        
        if (!markov_chain->is_last(chosen_node->data)) 
        {
            return chosen_node;
        }
    }
}

MarkovNode* get_next_random_node(MarkovNode *curr_markov_node)
{
    if (curr_markov_node == NULL || curr_markov_node->frequencies_list_length == 0)
    {
        return NULL;
    }

    int total_freq = 0;
    // Sum frequencies from the ARRAY
    for (size_t i = 0; i < curr_markov_node->frequencies_list_length; i++) 
    {
        total_freq += curr_markov_node->frequency_list[i].frequency;
    }

    if (total_freq == 0)
    {
        return NULL;
    }

    int rnd = get_random_number(total_freq);
    int cumulative = 0;

    // Find the weighted random node
    for (size_t i = 0; i < curr_markov_node->frequencies_list_length; i++) 
    {
        cumulative += curr_markov_node->frequency_list[i].frequency;
        if (rnd < cumulative)
        {
            return curr_markov_node->frequency_list[i].markov_node;
        }
    }
    return NULL;
}

void generate_random_sequence(MarkovChain *markov_chain, 
                              MarkovNode *first_node, int max_length)
{
    if (!markov_chain || !first_node)
    {
        return;
    }

    MarkovNode *curr = first_node;

    markov_chain->print_func(curr->data);

    for (int i = 1; i < max_length; i++)
    {
        if (markov_chain->is_last(curr->data))
        {
            break;
        }

        curr = get_next_random_node(curr);
        if (!curr)
        {
            break;
        }

        markov_chain->print_func(curr->data);
    }
    printf("\n");
}