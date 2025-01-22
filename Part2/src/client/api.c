#include "src/client/api.h"

#include "src/common/constants.h"
#include "src/common/protocol.h"
#include "src/common/io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <src/server/io.h>
#include <errno.h>

#define SUCCESS 0
#define FAILURE 1

ClientState client_state;

char* create_message(const char *key, int opcode) {
    char *message = (char*) malloc(sizeof(char)*(1+MAX_STRING_SIZE+1));

    memset(message, '\0', 1+MAX_STRING_SIZE+1);
    message[0] = (char) opcode;
    if (key != NULL)  {
        strcpy(message+1, key);
    }

    return message;
}

void delete_message(char *message) {
    free(message);
}

void print_response(int opcode, int response_code) {
    switch (opcode) {
        case OP_CODE_CONNECT:
            printf("Server returned %d for operation: connect\n", response_code);
            break;
        case OP_CODE_DISCONNECT:
            printf("Server returned %d for operation: disconnect\n", response_code);
            break;
        case OP_CODE_SUBSCRIBE:
            printf("Server returned %d for operation: subscribe\n", response_code);
            break;
        case OP_CODE_UNSUBSCRIBE:
            printf("Server returned %d for operation: unsubscribe\n", response_code);
            break;
    }
}

int kvs_connect(const char *req_pipe_path, const char *resp_pipe_path, const char *notif_pipe_path, const char *server_pipe_path) { 
    // assegurar que os pipes estão unlinked
    if (unlink(req_pipe_path) != 0 && errno != ENOENT) {
        perror("Erro de unlink\n");
        return FAILURE;
    }
    if (unlink(resp_pipe_path) != 0 && errno != ENOENT) {
        perror("Erro de unlink\n");
        return FAILURE;
    }
    if (unlink(notif_pipe_path) != 0 && errno != ENOENT) {
        perror("Erro de unlink\n");
        return FAILURE;
    }

    // criar os fifos dos clientes
    if (mkfifo(req_pipe_path, 0666) != 0 || mkfifo(resp_pipe_path, 0666) != 0 || mkfifo(notif_pipe_path, 0666) != 0) {
        perror("Erro ao criar os pipes do cliente\n");
        return FAILURE;
    }
    
    // Abrir o pipe do servidor
    int server_fd = open(server_pipe_path, O_WRONLY);
    if (server_fd == -1) {
        perror("Erro ao abrir o pipe do servidor\n");
        return FAILURE;
    }

    // Preparar mensagem de conexão
    char message[3 * MAX_PIPE_PATH_LENGTH + 1];
    memset(message, '\0', 3 * MAX_PIPE_PATH_LENGTH + 1);
    message[0] = (char) OP_CODE_CONNECT;
    strncpy(&message[1], req_pipe_path, strlen(req_pipe_path));
    strncpy(&message[1 + MAX_PIPE_PATH_LENGTH], resp_pipe_path, strlen(resp_pipe_path));
    strncpy(&message[1 + 2 * MAX_PIPE_PATH_LENGTH], notif_pipe_path, strlen(notif_pipe_path));
    
    // Enviar mensagem ao servidor
    if (write_all(server_fd, message, sizeof(message)) == -1) {
        perror("Erro ao escrever para o pipe server\n");
        close(server_fd);
        return FAILURE;
    }

    // abrir pipes do cliente
    int resp_fd = open(resp_pipe_path, O_RDONLY);
    if (resp_fd == -1) {
        perror("Erro ao abrir o response pipe do cliente\n");
        return FAILURE;
    }

    // Ler resposta do servidor
    char response[2];
    if (read_all(resp_fd, response, sizeof(response), NULL) == -1) {
        perror("Erro ao ler resposta do servidor\n");
        return FAILURE;
    }

    // Validar resposta
    if (response[0] != OP_CODE_CONNECT) {
        fprintf(stderr, "Resposta inválida: Código de resposta %d\n", response[1]);
        return FAILURE;
    }
    print_response(OP_CODE_CONNECT, response[1]);

    int req_fd = open(req_pipe_path, O_WRONLY);
    if (req_fd == -1) {
        perror("Erro ao abrir o requests pipe do cliente\n");
        return FAILURE;
    }
    int notif_fd = open(notif_pipe_path, O_RDONLY);
    if (notif_fd == -1) {
        perror("Erro ao abrir o notifications pipe do cliente\n");
        return FAILURE;
    }

    // Armazenar caminhos dos named pipes no cliente
    client_state.req_fd = req_fd;
    client_state.resp_fd = resp_fd;
    client_state.notif_fd = notif_fd;
    client_state.subscriptions = 0;

    // fechar pipe do servidor
    close(server_fd);

    return SUCCESS;
}

int kvs_disconnect(const char *req_pipe_path, const char *resp_pipe_path, const char *notif_pipe_path) {
    char message[1] = {(char) OP_CODE_DISCONNECT};
    if (write_all(client_state.req_fd, message, sizeof(message)) == -1) {
        perror("Erro ao enviar pedido de desconexão");
        return FAILURE;
    }

    char response[2];
    if (read_all(client_state.resp_fd, response, sizeof(response), NULL) == -1) {
        perror("Erro ao ler resposta do servidor");
        return FAILURE;
    }

    // Validar resposta do servidor
    if (response[0] != OP_CODE_DISCONNECT || response[1] != 0) {
        fprintf(stderr, "Erro ao desconectar: Código de resposta %d\n", response[1]);
        return FAILURE;
    }

    print_response(OP_CODE_DISCONNECT, response[1]);

    // fechar os pipes do cliente
    close(client_state.req_fd);
    close(client_state.resp_fd);
    close(client_state.notif_fd);

    // Apagar os named pipes do cliente
    if (unlink(req_pipe_path) == -1 || unlink(resp_pipe_path) == -1 || unlink(notif_pipe_path) == -1) {
        perror("Erro ao apagar os named pipes");
        return FAILURE;
    }

    return SUCCESS;
}

int kvs_subscribe(const char *key) {
    // Verificar número de subscrições máximo
    if (client_state.subscriptions >= MAX_NUMBER_SUB) {
        printf("Máximo de subscrições atingido.\n");
        return  FAILURE;
    }
    // escrever chave a subscrever e id do cliente para o server conseguir identificar
    char *message = create_message(key, OP_CODE_SUBSCRIBE);

    // verificar se da erro
    if (write_all(client_state.req_fd, message, 1+MAX_STRING_SIZE+1) == -1) {
        perror("Erro ao escrever para o pipe de pedidos\n");
        delete_message(message);
        return  FAILURE;
    }

    // Ler resposta do servidor
    char response[2];
    if (read_all(client_state.resp_fd, response, sizeof(response), NULL) == -1) {
        perror("Erro ao ler resposta do servidor\n");
        delete_message(message);
        return FAILURE;
    }

    // Validar resposta
    if ((int) response[0] != OP_CODE_SUBSCRIBE) {
        fprintf(stderr, "Resposta inválida: Código de resposta %d\n", response[1]);
        delete_message(message);
        return FAILURE;
    }
    print_response(OP_CODE_SUBSCRIBE, response[1]);
    client_state.subscriptions += 1;
    delete_message(message);

    return SUCCESS;
}

int kvs_unsubscribe(const char *key) {
    // Criar mensagem de unsubscribe
    char *message = create_message(key, OP_CODE_UNSUBSCRIBE);
    if (write_all(client_state.req_fd, message, 2+MAX_STRING_SIZE) == -1) {
        perror("Erro ao escrever para o pipe de resgistos");
        delete_message(message);
        return  FAILURE;
    }
    delete_message(message);

    // wait for response in response pipe
    // Ler resposta do servidor
    char response[2];
    if (read_all(client_state.resp_fd, response, sizeof(response), NULL) == -1) {
        perror("Erro ao ler resposta do servidor");
        return FAILURE;
    }

    // Validar resposta
    if (response[0] != OP_CODE_UNSUBSCRIBE) {
        fprintf(stderr, "Resposta inválida: Código de resposta %d\n", response[1]);
        return FAILURE;
    }
    print_response(OP_CODE_UNSUBSCRIBE, response[1]);
    client_state.subscriptions -= 1;

    return SUCCESS;
}