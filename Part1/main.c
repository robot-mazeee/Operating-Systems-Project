#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "constants.h"
#include "parser.h"
#include "operations.h"
#include "threads.h"

/**
 * @brief Reads and parses a single input file
 * @param pathname File to read
 * @param max_backups Maximum simultaneous backups allowed
*/
void readFile(char pathname[], int max_backups) {
  // open file
  int file = open(pathname, O_RDONLY);
  if (file == -1) {
    fprintf(stderr, "Failed to open file %s\n", pathname);
    return;
  }

  int backup_num = 1, simultaneous_backups = 0;
  char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
  char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
  unsigned int delay;
  size_t num_pairs;
  
  // get .out filename
  char pathname_out[PATH_MAX];
  strcpy(pathname_out, pathname);
  char *dotPos = strrchr(pathname_out, '.');
  strcpy(dotPos, ".out");

  // open output file
  int file_out = open(pathname_out, O_CREAT | O_WRONLY, 0644);

  // read file
  int running = 1;
  while(running) {
    switch (get_next(file)) {
      case CMD_WRITE:
        num_pairs = parse_write(file, keys, values, MAX_WRITE_SIZE, MAX_STRING_SIZE);
        if (num_pairs == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        if (kvs_write(num_pairs, keys, values)) {
          fprintf(stderr, "Failed to write pair\n");
        }
        break;

      case CMD_READ:
        num_pairs = parse_read_delete(file, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

        if (num_pairs == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_read(num_pairs, keys, file_out)) {
          fprintf(stderr, "Failed to read pair\n");
        }
        break;

      case CMD_DELETE:
        num_pairs = parse_read_delete(file, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

        if (num_pairs == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_delete(num_pairs, keys, file_out)) {
          fprintf(stderr, "Failed to delete pair\n");
        }
        break;

      case CMD_SHOW:
        kvs_show(file_out);
        break;

      case CMD_WAIT:
        if (parse_wait(file, &delay, NULL) == -1) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (delay > 0) {
          kvs_wait(file_out, delay);
        }
        break;

      case CMD_BACKUP:
        if (kvs_backup(pathname, max_backups, &simultaneous_backups, backup_num)) {
          fprintf(stderr, "Failed to perform backup.\n");
        } else backup_num++;
        break;

      case CMD_INVALID:
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_HELP: {
        const char *content = 
              "Available commands:\n"
              "  WRITE [(key,value),(key2,value2),...]\n"
              "  READ [key,key2,...]\n"
              "  DELETE [key,key2,...]\n"
              "  SHOW\n"
              "  WAIT <delay_ms>\n"
              "  BACKUP\n"
              "  HELP\n"
        ;
        write(file_out, content, strlen(content));
        break;
      }
        
      case CMD_EMPTY:
        break;

      case EOC:
        running = 0;
        break;
    }
  }
  // cleanup
  close(file);
  close(file_out);
}

/**
 * @brief Read a single input file using a thread
 * @param args readFile args
*/
void* threadWorker(void* args) {
  ThreadArgs* threadArgs = (ThreadArgs*)args;

  readFile(threadArgs->pathname, threadArgs->max_backups);

  free(threadArgs);
  return NULL;
}

/**
 * @brief Read all .job files in directory
 * @param directory Directory to read
 * @param max_backups Maximum simultaneous backups allowed
 * @param max_threads Maximum simultaneous threads allowed
*/
void readDir(char directory[], int max_backups, int max_threads) {
  DIR *dir = opendir(directory);
  if (dir == NULL) {
    fprintf(stderr, "Failed to open directory\n");
    return;
  }

  // array of threads
  pthread_t threads[max_threads];
  int thread_count = 0;

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    char pathname[PATH_MAX];
    const char *filename = entry->d_name;

    // check if the filename ends with ".job"
    size_t len = strlen(filename);
    if (!(len > 4 && strcmp(filename + len - 4, ".job") == 0)) {
        continue;
    }
    // get file path
    snprintf(pathname, sizeof(pathname), "%s/%s", directory, filename);

    // allocates arguments for threads
    ThreadArgs* threadArgs = (ThreadArgs*) malloc(sizeof(ThreadArgs));
    strcpy(threadArgs->pathname, pathname);
    threadArgs->max_backups = max_backups;

    // creates a new thread
    if (pthread_create(&threads[thread_count++], NULL, threadWorker, (void*) threadArgs) != 0) {
        fprintf(stderr, "Failed to create thread\n");
        free(threadArgs);
        continue;
    }

    // if max_threads is reached, waits for all threads to terminate 
    if (thread_count >= max_threads) {
      for (int i = 0; i < thread_count; i++) {
          pthread_join(threads[i], NULL);
      }
      thread_count = 0;
    }
  }
  // waits for all threads to terminate 
  for (int i = 0; i < thread_count; i++) {
      pthread_join(threads[i], NULL);
  }
  // cleanup
  closedir(dir);
  if (kvs_terminate()) {
    return;
  }
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "Wrong number of arguments.%d\n", argc);
    return 1;
  }
  if (kvs_init()) {
    fprintf(stderr, "Failed to initialize KVS\n");
    return 1;
  }

  char *directory = argv[1];
  int backups = atoi(argv[2]);
  int max_threads = atoi(argv[3]);

  readDir(directory, backups, max_threads);
  return 0;
}