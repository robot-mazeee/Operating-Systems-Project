#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"
#include "src/client/api.h"
#include "src/common/io.h"

pthread_t notif_thread;

void *notif_task(void* arg) {
  char *notif_pipe_path = (char*) arg;
  int notif_fd = open(notif_pipe_path, O_RDONLY);
  if (notif_fd == -1) {
    perror("Erro ao abrir pipe de notificações do cliente\n");
    pthread_exit(NULL);
  }
  while (1) {
    char key[MAX_STRING_SIZE + 1];
    char value[MAX_STRING_SIZE + 1];
    char message[2*(MAX_STRING_SIZE+1)];
    int result = read_all(notif_fd, message, sizeof(message), NULL);
    if (result == -1) {
      perror("Erro ao ler notificação do servidor\n");
      continue;
    }
    else if (result == 0) {
      return NULL;
    }
    else {
      strncpy(key, message, MAX_STRING_SIZE+1);
      strncpy(value, message+MAX_STRING_SIZE+1, MAX_STRING_SIZE+1);

      printf("(<%s>,<%s>)\n", key, value);
    }
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <client_unique_id> <register_pipe_path>\n",
            argv[0]);
    return 1;
  }

  char *server_pipe_path = argv[2];

  // criar estes pipes, abri-los e mandar os paths para o server para o server os poder abrir
  char req_pipe_path[256] = "/tmp/req";
  char resp_pipe_path[256] = "/tmp/resp";
  char notif_pipe_path[256] = "/tmp/notif";

  strncat(req_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(resp_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(notif_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));

  // conectar o cliente 
  kvs_connect(req_pipe_path, resp_pipe_path, notif_pipe_path, server_pipe_path);

  // Criar a thread de notificações
  if (pthread_create(&notif_thread, NULL, notif_task, (void*) notif_pipe_path) != 0) {
      perror("Erro ao criar a thread de notificações\n");
      return FAILURE;
  }

  char keys[MAX_NUMBER_SUB][MAX_STRING_SIZE] = {0};
  unsigned int delay_ms;
  size_t num;

  while (1) {
    switch (get_next(STDIN_FILENO)) {
    case CMD_DISCONNECT:
      if (kvs_disconnect(req_pipe_path, resp_pipe_path, notif_pipe_path) != 0) {
        fprintf(stderr, "Failed to disconnect from server\n");
        return FAILURE;
      }
      // end notifications thread
      pthread_join(notif_thread, NULL);
      printf("Disconnected\n");
      return SUCCESS;

    case CMD_SUBSCRIBE:
      num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
      if (num == 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (kvs_subscribe(keys[0])) {
        fprintf(stderr, "Command subscribe failed\n");
      }

      break;

    case CMD_UNSUBSCRIBE:
      num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
      if (num == 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (kvs_unsubscribe(keys[0])) {
        fprintf(stderr, "Command unsubscribe failed\n");
      }

      break;

    case CMD_DELAY:
      if (parse_delay(STDIN_FILENO, &delay_ms) == -1) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (delay_ms > 0) {
        printf("Waiting...\n");
        delay(delay_ms);
      }
      break;

    case CMD_INVALID:
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      break;

    case CMD_EMPTY:
      break;

    case EOC:
      kvs_disconnect(req_pipe_path, resp_pipe_path, notif_pipe_path);
      break;
    }
  }

  pthread_join(notif_thread, NULL);

  return SUCCESS;
}