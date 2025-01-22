#include "kvs.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <src/common/io.h>

// Hash function based on key initial.
// @param key Lowercase alphabetical string.
// @return hash.
// NOTE: This is not an ideal hash function, but is useful for test purposes of the project
int hash(const char *key) {
    int firstLetter = tolower(key[0]);
    if (firstLetter >= 'a' && firstLetter <= 'z') {
        return firstLetter - 'a';
    } else if (firstLetter >= '0' && firstLetter <= '9') {
        return firstLetter - '0';
    }
    return -1; // Invalid index for non-alphabetic or number strings
}

/// @brief Initializes clients of a new key
/// @param key
void initKeyClients(KeyNode** keyNode) {
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
        (*keyNode)->clients[i] = -1;
    }
}

struct HashTable* create_hash_table() {
  HashTable *ht = malloc(sizeof(HashTable));
  if (!ht) return NULL;
  for (int i = 0; i < TABLE_SIZE; i++) {
      ht->table[i] = NULL;
  }
  return ht;
}

int write_pair(HashTable *ht, const char *key, const char *value) {
    int index = hash(key);

    KeyNode *keyNode = ht->table[index];

    // Search for the key node
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            free(keyNode->value);
            keyNode->value = strdup(value);
            notify(keyNode, keyNode->value);
            return SUCCESS;
        }
        keyNode = keyNode->next; // Move to the next node
    }

    // Key not found, create a new key node
    keyNode = malloc(sizeof(KeyNode));
    keyNode->key = strdup(key); // Allocate memory for the key
    keyNode->value = strdup(value); // Allocate memory for the value
    initKeyClients(&keyNode); // inicializa os clientes subscritos a essa chave
    keyNode->next = ht->table[index]; // Link to existing nodes
    ht->table[index] = keyNode; // Place new key node at the start of the list

    return SUCCESS;
}

char* read_pair(HashTable *ht, const char *key) {
    int index = hash(key);
    KeyNode *keyNode = ht->table[index];
    char* value;

    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            value = strdup(keyNode->value);
            return value; // Return copy of the value if found
        }
        keyNode = keyNode->next; // Move to the next node
    }
    return NULL; // Key not found
}

int delete_pair(HashTable *ht, const char *key) {
    int index = hash(key);
    KeyNode *keyNode = ht->table[index];
    KeyNode *prevNode = NULL;

    // Search for the key node
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            // Key found; delete this node
            if (prevNode == NULL) {
                // Node to delete is the first node in the list
                ht->table[index] = keyNode->next; // Update the table to point to the next node
            } else {
                // Node to delete is not the first; bypass it
                prevNode->next = keyNode->next; // Link the previous node to the next node
            }
            // Free the memory allocated for the key and value
            notify(keyNode, NULL); // notify subscribed clients of deletion
            free(keyNode->key);
            free(keyNode->value);
            free(keyNode); // Free the key node itself
            return 0; // Exit the function
        }
        prevNode = keyNode; // Move prevNode to current node
        keyNode = keyNode->next; // Move to the next node
    }
    
    return 1;
}

KeyNode* get_key_node(HashTable *ht, const char *key) {
    // get hash_table index
    if (ht == NULL || key == NULL) {
        return NULL;
    }
    int index = hash(key);
    KeyNode *keyNode = ht->table[index];

    // search key in hash_table index
    while (keyNode != NULL && strcmp(keyNode->key, key)) {
        keyNode = keyNode->next;
    }

    return keyNode;
}

int notify(KeyNode *keyNode, char *value) {
    // Escrever mensagem para os notifications pipes dos clientes
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
        if (keyNode->clients[i] != -1) {
            char message[2*(MAX_STRING_SIZE+1)];
            // Construir mensagem
            memset(message, '\0', sizeof(message));
            strcpy(message, keyNode->key);
            if (value == NULL) {
                strcpy(message + MAX_STRING_SIZE + 1, "DELETED");
            } else {
                strcpy(message + MAX_STRING_SIZE + 1, value);
            }
            if (write_all(keyNode->clients[i], message, sizeof(message)) == -1){
                perror("Erro ao escrever para o pipe de notificações\n");
                return  FAILURE;
            }
        }
    }
  return SUCCESS;
}

void free_table(HashTable *ht) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        KeyNode *keyNode = ht->table[i];
        while (keyNode != NULL) {
            KeyNode *temp = keyNode;
            keyNode = keyNode->next;
            free(temp->key);
            free(temp->value);
            free(temp);
        }
    }
    free(ht);
}