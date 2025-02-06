#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h> 
#include <sys/wait.h>
#include <pthread.h>

#include "kvs.h"
#include "constants.h"

static struct HashTable* kvs_table = NULL;

// lock for each table entry
pthread_rwlock_t table_locks[TABLE_SIZE] = {PTHREAD_RWLOCK_INITIALIZER};

// lock for accessing current simultaneous backups
pthread_mutex_t backups_lock = PTHREAD_MUTEX_INITIALIZER;

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

int kvs_init() {
  if (kvs_table != NULL) {
    fprintf(stderr, "KVS state has already been initialized\n");
    return 1;
  }

  kvs_table = create_hash_table();
  return kvs_table == NULL;
}

int kvs_terminate() {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }

  free_table(kvs_table);
  return 0;
}

void sort_keys_values(size_t num_pairs, char keys[][MAX_STRING_SIZE], char values[][MAX_STRING_SIZE]) {
  for (size_t i = 0; i < num_pairs; i++) {
    size_t min = i;
    for (size_t j = i+1; j < num_pairs; j++) {
      if(strcmp(keys[min], keys[j]) > 0) {
          min = j;   
      }
    }
    if (min != i) {
      char temp[MAX_STRING_SIZE];
      strcpy(temp, keys[i]);
      strcpy(keys[i], keys[min]);
      strcpy(keys[min], temp);
      char temp2[MAX_STRING_SIZE];
      strcpy(temp2, values[i]);
      strcpy(values[i], values[min]);
      strcpy(values[min], temp2);
    }
  }
}

int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE], char values[][MAX_STRING_SIZE]) {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }

  // stops dead-locks between threads
  sort_keys_values(num_pairs, keys, values);

  // acquire locks correspondent to each table entry
  int locked_indices[TABLE_SIZE] = {0};
  for (size_t i = 0; i < num_pairs; i++) {
    int index = hash(keys[i]);
    if (!locked_indices[index]) {
      locked_indices[index] = 1;
      pthread_rwlock_wrlock(&table_locks[index]);
    }
  }
  
  for (size_t i = 0; i < num_pairs; i++) {
    if (write_pair(kvs_table, keys[i], values[i]) != 0) {
      fprintf(stderr, "Failed to write keypair (%s,%s)\n", keys[i], values[i]);
    }
  }

  // free locks
  for (size_t i = 0; i < num_pairs; i++) {
    int index = hash(keys[i]);
    if (locked_indices[index]) {
      locked_indices[index] = 0;
      pthread_rwlock_unlock(&table_locks[index]);
    }
  }

  return 0;
}

void sort_keys(size_t num_pairs, char keys[][MAX_STRING_SIZE]) {
  for (size_t i = 0; i < num_pairs; i++) {
    size_t min = i;
    for (size_t j = i+1; j < num_pairs; j++) {
      if(strcmp(keys[min], keys[j]) > 0) {
          min = j;   
      }
    }
    if (min != i) {
      char temp[MAX_STRING_SIZE];
      strcpy(temp, keys[i]);
      strcpy(keys[i], keys[min]);
      strcpy(keys[min], temp);
    }
  }
}

int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int file_out) {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }

  // prevents dead-locks between threads
  sort_keys(num_pairs, keys);

  // acquire locks correspondent to each table entry
  int locked_indices[TABLE_SIZE] = {0};
  for (size_t i = 0; i < num_pairs; i++) {
    int index = hash(keys[i]);
    if (!locked_indices[index]) {
      locked_indices[index] = 1;
      pthread_rwlock_rdlock(&table_locks[index]);
    }
  }

  // write to output file
  write(file_out, "[", 1);
  for (size_t i = 0; i < num_pairs; i++) {
    char* result = read_pair(kvs_table, keys[i]);
    if (result == NULL) {
      char content[MAX_WRITE_SIZE];
      sprintf(content, "(%s,KVSERROR)", keys[i]);
      write(file_out, content, strlen(content));
    } else {
      char content[MAX_WRITE_SIZE];
      sprintf(content, "(%s,%s)", keys[i], result);
      write(file_out, content, strlen(content));
    }
    free(result);
  }
  write(file_out, "]\n", 2);

  // free locks
  for (size_t i = 0; i < num_pairs; i++) {
    int index = hash(keys[i]);
    if (locked_indices[index]) {
      locked_indices[index] = 0;
      pthread_rwlock_unlock(&table_locks[index]);
    }
  }
  return 0;
}

int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int file_out) {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }

  // prevents dead-locks between threads
  sort_keys(num_pairs, keys);

  // acquire locks correspondent to each table entry
  int locked_indices[TABLE_SIZE] = {0};
  for (size_t i = 0; i < num_pairs; i++) {
    int index = hash(keys[i]);
    if (!locked_indices[index]) {
      locked_indices[index] = 1;
      pthread_rwlock_wrlock(&table_locks[index]);
    }
  }

  // delete pairs
  int aux = 0;
  for (size_t i = 0; i < num_pairs; i++) {
    if (delete_pair(kvs_table, keys[i]) != 0) {
      if (!aux) {
        write(file_out, "[", 1);
        aux = 1;
      }
      char content[MAX_WRITE_SIZE];
      sprintf(content, "(%s,KVSMISSING)", keys[i]);
      write(file_out, content, strlen(content));
    }
  }
  if (aux) {
    write(file_out, "]\n", 2);
  }

  // free locks
  for (size_t i = 0; i < num_pairs; i++) {
    int index = hash(keys[i]);
    if (locked_indices[index]) {
      locked_indices[index] = 0;
      pthread_rwlock_unlock(&table_locks[index]);
    }
  }

  return 0;
}

void kvs_show(int file_out) {
  // acquire locks for all table entries
  for (int i = 0; i < TABLE_SIZE; i++) {
    pthread_rwlock_rdlock(&table_locks[i]);
  }

  // show table contents
  for (int i = 0; i < TABLE_SIZE; i++) {
    KeyNode *keyNode = kvs_table->table[i];
    while (keyNode != NULL) {
      char content[MAX_WRITE_SIZE];
      sprintf(content, "(%s, %s)\n", keyNode->key, keyNode->value);
      write(file_out, content, strlen(content));
      keyNode = keyNode->next; // Move to the next node
    }
  }

  // free locks
  for (int i = 0; i < TABLE_SIZE; i++) {
    pthread_rwlock_unlock(&table_locks[i]);
  }
}

int kvs_backup(char pathname[], int max_backups, int *simultaneous_backups, int backup_num) { 
  int pid, status;

  // acquire lock for backups
  pthread_mutex_lock(&backups_lock);
  // if simultaneous_backups exceed the maximum, wait for one backup to terminate
  if (*simultaneous_backups > max_backups) {
    pid = wait(&status);
    *simultaneous_backups -= 1;
  }
  // increment simultaneous_backups and release lock
  *simultaneous_backups += 1;
  pthread_mutex_unlock(&backups_lock);

  pid = fork();
  // child process
  if (pid == 0) {
    // get backup file path
    char filename_out[MAX_JOB_FILE_NAME_SIZE], pathname_out[PATH_MAX];
    strcpy(filename_out, pathname);
    char *dotPos = strrchr(filename_out, '.');
    *dotPos = '\0';
    snprintf(pathname_out, sizeof(filename_out)+sizeof(backup_num)+4, "%s-%d.bck", filename_out, backup_num);
    
    // open backup file
    int file_out = open(pathname_out, O_CREAT | O_WRONLY, 0644);

    // perform backup
    kvs_show(file_out);

    // cleanup and exit
    close(file_out);
    kvs_terminate();
    _exit(0);
  }
  // parent process
  else {
    return 0;
  }
  
  return 0;
}

void kvs_wait(int file_out, unsigned int delay_ms) {
  char wait[] = "Waiting...\n";
  write(file_out, wait, strlen(wait));
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}