#ifndef KEY_VALUE_STORE_H
#define KEY_VALUE_STORE_H

#define TABLE_SIZE 26

#include "src/common/constants.h"
#include <stddef.h>

typedef struct KeyNode {
    char *key;
    char *value;
    // file descriptors for subscribed client's notifications pipes
    int clients[MAX_SESSION_COUNT];
    struct KeyNode *next;
} KeyNode;

typedef struct HashTable {
    KeyNode *table[TABLE_SIZE];
} HashTable;

/// @brief Hashing function to transform the key of the pair into an index
/// @param key Key of the pair
/// @return Index of key to hash_table
int hash(const char *key);

/// @brief Initializes all clients subscribed to key with -1
/// @param keyNode keyNode to initialize
void initKeyClients(KeyNode** keyNode);

/// Creates a new event hash table.
/// @return Newly created hash table, NULL on failure
struct HashTable *create_hash_table();

/// Appends a new key value pair to the hash table.
/// @param ht Hash table to be modified.
/// @param key Key of the pair to be written.
/// @param value Value of the pair to be written.
/// @return 0 if the node was appended successfully, 1 otherwise.
int write_pair(HashTable *ht, const char *key, const char *value);

/// Deletes the value of given key.
/// @param ht Hash table to delete from.
/// @param key Key of the pair to be deleted.
/// @return 0 if the node was deleted successfully, 1 otherwise.
char* read_pair(HashTable *ht, const char *key);

/// Appends a new node to the list.
/// @param list Event list to be modified.
/// @param key Key of the pair to read.
/// @return 0 if the node was appended successfully, 1 otherwise.
int delete_pair(HashTable *ht, const char *key);

/// Frees the hashtable.
/// @param ht Hash table to be deleted.
void free_table(HashTable *ht);

/// @brief Searches for a keyNode in the hashtable
/// @param ht hashtable
/// @param key key to search for
/// @return keyNode with a certain key
KeyNode* get_key_node(HashTable *ht, const char *key);

/// @brief notifies all subscribed clients of a change in key
/// @param keyNode keyNode changed
/// @param value value key was changed to
/// @return 0 if operation is successful, 1 otherwise
int notify(KeyNode *keyNode, char *value);

#endif  // KVS_H
