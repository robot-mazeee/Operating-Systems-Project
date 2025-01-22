#ifndef CLIENT_API_H
#define CLIENT_API_H

#include <stddef.h>

#include "src/common/constants.h"

// Estrutura para armazenar os pipes do cliente
typedef struct {
    int req_fd;
    int resp_fd;
    int notif_fd;
    int subscriptions;
} ClientState;

/// Connects to a kvs server.
/// @param req_pipe_path Path to the name pipe to be created for requests.
/// @param resp_pipe_path Path to the name pipe to be created for responses.
/// @param server_pipe_path Path to the name pipe where the server is listening.
/// @return 0 if the connection was established successfully, 1 otherwise.
int kvs_connect(char const *req_pipe_path, char const *resp_pipe_path, char const *notif_pipe_path, char const *server_pipe_path);

/// Disconnects from an KVS server.
/// @return 0 in case of success, 1 otherwise.
int kvs_disconnect(const char *req_pipe_path, const char *resp_pipe_path, const char *notif_pipe_path);

/// Requests a subscription for a key
/// @param key Key to be subscribed
/// @return 1 if the key was subscribed successfully (key existing), 0
/// otherwise.
int kvs_subscribe(const char *key);

/// Remove a subscription for a key
/// @param key Key to be unsubscribed
/// @return 0 if the key was unsubscribed successfully  (subscription existed
/// and was removed), 1 otherwise.
int kvs_unsubscribe(const char *key);

/// @brief imprime a resposta do servidor a uma certa operação
/// @param opcode opcode da operação
/// @param response_code código de resposta da operação
void print_response(int opcode, int response_code);

/// @brief cria uma mensagem para enviar ao sevidor
/// @param key chave para incluir na mensagem
/// @param opcode opcode da operação
/// @return mensagem criada
char* create_message(const char *key, int opcode);

/// @brief apaga uma mensagem dinamicamente alocada e toda a memória a ela 
/// associada
/// @param message mensagem a ser apagada
void delete_message(char *message);

#endif // CLIENT_API_H
