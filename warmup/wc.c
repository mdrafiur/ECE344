#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include "common.h"
#include "wc.h"

void wc_insert_word(struct wc *wc, char *word);

typedef struct list_t {
    char *key;
    int numOccured;
    struct list_t *next;
} list;

struct wc {
    long tableSize; // size of the hash table
    list **table;
};

unsigned int
hash(struct wc *hashtable, char *str)
{
    unsigned int hashval = 0;
    
    for(; *str != '\0'; str++) hashval ^= (hashval << 5) + (hashval >> 2) + *str;

    /* we then return the hash value mod the hashtable size so that it will
    * fit into the necessary range
    */
    return hashval % hashtable->tableSize;
}

struct wc *
wc_init(char *word_array, long size)
{
    struct wc *wc;
    long i;
    int len = 0;
    char word[1024];
    char c;

    wc = (struct wc *)malloc(sizeof(struct wc));
    assert(wc);

    /* Hash table's size */
    wc->tableSize = 402653189;

    /* Attempt to allocate memory for the table itself */
    if ((wc->table = malloc(sizeof(list *) * wc->tableSize)) == NULL) {
        return NULL;
    }

    /* Initialize the elements of the table */
    for(i = 0; i < wc->tableSize; i++) {
        wc->table[i] = NULL;
    }
    
    for (i = 0; i < size; i++) {
        /* whitespace finishes a word. */
        c = word_array[i];
        if (isspace(c)) {
            if (len > 0) {
                word[len] = 0;
                wc_insert_word(wc, word);
                len = 0;
            }
        } else {
            word[len] = c;
            len++;
        }
    }
    if (len > 0) {
        wc_insert_word(wc, word);
    }

    return wc;
}

bool
lookup_word(struct wc *hashTable, char *word, unsigned int hashVal)
{
    list *list;

    for(list = hashTable->table[hashVal]; list != NULL; list = list->next) 
    {
        if (strcmp(word, list->key) == 0)
        {
            list->numOccured++;
            return true;
        }
    }

    return false;
}

void
wc_insert_word(struct wc *wc, char *word)
{
    list *newList;

    unsigned int hashVal = hash(wc, word);

    // Checks if the word already exist and If yes return
    if (lookup_word(wc, word, hashVal)) 
        return;

    /* Attempt to allocate memory for list */
    newList = malloc(sizeof(list));
    assert(newList);

    /* Insert into list */
    newList->key = strdup(word);
    newList->numOccured = 1;
    newList->next = wc->table[hashVal];
    wc->table[hashVal] = newList;

    return;
}

void
wc_output(struct wc *wc)
{
    int i;
    list *list;

    if (!wc) return;

    for(i = 0; i < wc->tableSize; i++) {
        list = wc->table[i];
        while(list != NULL) {
            printf("%s:%d\n", list->key, list->numOccured);
            list = list->next;
        }
    }
}

void 
list_destroy(list *lt)
{
    if(lt->next == NULL)
    {
        free(lt->key);
        free(lt);
        return;
    }

    list_destroy(lt->next);
    free(lt->key);
    free(lt);
    return;
}

void
wc_destroy(struct wc *wc)
{
    long i;

    for(i = 0; i < wc->tableSize; i++) {
        if(wc->table[i] != NULL) {
            list_destroy(wc->table[i]);
            wc->table[i] = NULL;
        }
    }
    free(wc->table);
    free(wc);
}

