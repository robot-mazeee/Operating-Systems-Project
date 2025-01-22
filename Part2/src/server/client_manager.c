#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <errno.h>
#include <signal.h>

#include "constants.h"
#include "parser.h"
#include "operations.h"
#include <src/server/client_manager.h>
#include "src/common/constants.h"
#include "src/common/protocol.h"
#include "src/common/io.h"
#include "src/server/operations.h"

// Semáforos e Mutex
sem_t space_in_buffer; // Espaço disponível no buffer
sem_t info_to_read; // Informação disponível para ler

pthread_mutex_t buffer_mutex; // Proteção ao buffer partilhado
pthread_mutex_t buffer_index_mutex; // Proteção ao índice partilhado

// Buffer partilhado
char buffer[MAX_PIPE_PATH_LENGTH * 3 * MAX_SESSION_COUNT];
int buffer_index = 0;

// Array de threads para gerenciar clientes
pthread_t manager_threads[MAX_SESSION_COUNT];
pthread_t host_thread;

// Lista de clientes com os fd
Client *clients[MAX_SESSION_COUNT] = {NULL};

// sinal
volatile sig_atomic_t sigusr1_received = 0;

void sigusr1_handler(int sig) {
    if (sig == SIGUSR1) {
        // Apagar todos os clientes
        for (int i = 0; i < MAX_SESSION_COUNT; i++) {
            if (clients[i] != NULL) {
                delete_client(&clients[i]);
            }
        }
        printf("Todos os clientes desconectados.\n");
    }
}

Client* add_client(int req_fd, int resp_fd, int notif_fd) {
    // Criar novo cliente
    Client *client = (Client*) malloc(sizeof(Client));
    client->req_fd = req_fd;
    client->resp_fd = resp_fd;
    client->notif_fd = notif_fd;

    // Adicionar cliente
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
        if (clients[i] == NULL) {
            clients[i] = client;
            break;
        }
    }
    return client;
}

void delete_client(Client **client) {
    // Apagar todas as subscrições do cliente
    delete_all_subs((*client)->notif_fd);
    // Apagar o cliente da lista e libertar memória a ele associada
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
        if (clients[i] != NULL) {
            int req_fd = clients[i]->req_fd;
            int resp_fd = clients[i]->resp_fd;
            int notif_fd = clients[i]->notif_fd;
            if (req_fd == (*client)->req_fd && resp_fd == (*client)->resp_fd && notif_fd == (*client)->notif_fd) {
                close_pipes((*client)->req_fd, (*client)->resp_fd, (*client)->notif_fd);
                free(*client);
                clients[i] = NULL;
                break;
            }
        }
    }
    // Post de espaço no buffer, ganhamos uma sessão livre para preencher com
    // um novo cliente
    sem_post(&space_in_buffer);
}

void close_pipes(int req_fd, int resp_fd, int notif_fd) {
    close(req_fd);
    close(resp_fd);
    close(notif_fd);
}

// Função das threads gestoras
void* manager_thread() {
    sigset_t set;
    // Block SIGUSR1 in this thread
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        perror("Erro ao ignorar o sinal\n");
        pthread_exit(NULL);
    }

    while (1) {
        char req_pipe[MAX_PIPE_PATH_LENGTH];
        char resp_pipe[MAX_PIPE_PATH_LENGTH];
        char notif_pipe[MAX_PIPE_PATH_LENGTH];

        // Esperar por informação para ler
        sem_wait(&info_to_read);
        
        // secção crítica: mudar índice de leitura do buffer
        pthread_mutex_lock(&buffer_index_mutex);
        buffer_index -= 3 * MAX_PIPE_PATH_LENGTH;
        pthread_mutex_unlock(&buffer_index_mutex);

        // Secção crítica: ler do buffer
        pthread_mutex_lock(&buffer_mutex);
        strncpy(req_pipe, buffer + buffer_index, MAX_PIPE_PATH_LENGTH);
        strncpy(resp_pipe, buffer + buffer_index + MAX_PIPE_PATH_LENGTH, MAX_PIPE_PATH_LENGTH);
        strncpy(notif_pipe, buffer + buffer_index + 2 * MAX_PIPE_PATH_LENGTH, MAX_PIPE_PATH_LENGTH);
        pthread_mutex_unlock(&buffer_mutex); 

        // Post de espaço no buffer
        sem_post(&space_in_buffer);

        // Abrir o pipe de respostas do cliente
        int resp_fd = open(resp_pipe, O_WRONLY);
        if (resp_fd == -1) {
            perror("Erro a abrir response pipe\n");
            continue;
        }
        // mandar mensagem de sucesso do connect ao cliente
        char success_message[2] = {(char) OP_CODE_CONNECT, (char) SUCCESS};
        if (write_all(resp_fd, success_message, sizeof(success_message)) == -1){
            perror("Erro ao enviar mensagem de resposta da conexão\n");
            continue;
        }

        // Abrir os restantes pipes do cliente
        int req_fd = open(req_pipe, O_RDONLY);
        if (req_fd == -1) {
            perror("Erro a abrir requests pipe\n");
            continue;
        }
        int notif_fd = open(notif_pipe, O_WRONLY);
        if (notif_fd == -1) {
            perror("Erro a abrir notifications pipe\n");
            continue;
        }

        Client *current_client = add_client(req_fd, resp_fd, notif_fd);
        int is_connected = 1;
        // Ler request pipe e processar os pedidos
        while (is_connected) {
            char opcode[1];
            int read_result = read_all(req_fd, opcode, sizeof(opcode), NULL);
            if (read_result == -1 || read_result == 0) {
                delete_client(&current_client);
                is_connected = 0;
                break;
            }
            
            char message[2];
            int result;
            switch ((int) opcode[0]) {
                case OP_CODE_DISCONNECT: {
                    // Enviar resposta
                    message[0] = (char) OP_CODE_DISCONNECT;
                    message[1] = (char) SUCCESS;
                    if (write_all(resp_fd, message, sizeof(message)) == -1) {
                        perror("Erro ao enviar mensagem para o cliente\n");
                    }
                    // Apagar o cliente
                    delete_client(&current_client);
                    is_connected = 0;
                    break;
                }
                
                case OP_CODE_SUBSCRIBE: {
                    // Ler a chave a subscrever
                    char key[MAX_STRING_SIZE+1];
                    if (read_all(req_fd, key, sizeof(key), NULL) == -1) {
                        perror("Erro ao ler request do cliente\n");
                        delete_client(&current_client);
                        is_connected = 0;
                        break;
                    }
                    result = subscribe_key(key, notif_fd);

                    // Mensagem de resposta
                    message[0] = (char) OP_CODE_SUBSCRIBE;
                    message[1] = (char) result;
                    if (write_all(resp_fd, message, sizeof(message)) == -1) {
                        perror("Erro ao enviar mensagem para o cliente\n");
                        delete_client(&current_client);
                        is_connected = 0;
                        break;
                    }
                    break;
                }

                case OP_CODE_UNSUBSCRIBE: {
                    // Ler a chave 
                    char key_to_unsub[MAX_STRING_SIZE+1];
                    if (read_all(req_fd, key_to_unsub, sizeof(key_to_unsub), NULL) == -1) {
                        perror("Erro ao enviar mensagem para o cliente\n");
                        delete_client(&current_client);
                        is_connected = 0;
                        break;
                    }
                    result = unsubscribe_key(key_to_unsub, notif_fd);

                    // Mensagem de resposta
                    message[0] = (char) OP_CODE_UNSUBSCRIBE;
                    message[1] = (char) result;
                    if (write_all(resp_fd, message, sizeof(message)) == -1) {
                        perror("Erro ao enviar mensagem para o cliente");
                        delete_client(&current_client);
                        is_connected = 0;
                        break;
                    }
                    break;
                }
            }  
        }
    }
    
    return NULL;
}

// Função da tarefa anfitriã
void* host_task(void* arg) {
    // associar handler ao sinal
    if (signal(SIGUSR1, sigusr1_handler) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }

    char* server_path = (char*) arg;

    // create server pipe
    if (unlink(server_path) != 0 && errno != ENOENT) {
        perror("Erro ao remover pipe existente\n");
        pthread_exit(NULL); // Exit thread cleanly
    }
    if (mkfifo(server_path, 0666) == -1) {
        perror("Erro ao criar o pipe do server\n");
    }
    
    // Criar threads gestoras
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
        if (pthread_create(&manager_threads[i], NULL, manager_thread, NULL) != 0) {
            perror("Erro ao criar thread gestora\n");
            exit(EXIT_FAILURE);
        }
    }

    // Abrir pipe do servidor
    int server_fd = open(server_path, O_RDWR);
    while (1) {
        // ler continuamente do server pipe
        char request[3 * MAX_PIPE_PATH_LENGTH + 1];
        if (read_all(server_fd, request, sizeof(request), NULL) == -1) {
            perror("Erro ao ler pedido de conexão com o servidor\n");
            continue;
        } 

        // le os pipes do cliente enviados na resposta 
        char req_pipe[MAX_PIPE_PATH_LENGTH];
        char resp_pipe[MAX_PIPE_PATH_LENGTH];
        char notif_pipe[MAX_PIPE_PATH_LENGTH];
        strncpy(req_pipe, request + 1, MAX_PIPE_PATH_LENGTH);
        strncpy(resp_pipe, request + 1 + MAX_PIPE_PATH_LENGTH, MAX_PIPE_PATH_LENGTH);
        strncpy(notif_pipe, request + 1 + 2*MAX_PIPE_PATH_LENGTH, MAX_PIPE_PATH_LENGTH);

        // Esperar por espaço no buffer
        sem_wait(&space_in_buffer);

        // Quando tem espaço, escrever no buffer
        pthread_mutex_lock(&buffer_mutex);
        strncpy(buffer + buffer_index, req_pipe, MAX_PIPE_PATH_LENGTH);
        strncpy(buffer + buffer_index + MAX_PIPE_PATH_LENGTH, resp_pipe, MAX_PIPE_PATH_LENGTH);
        strncpy(buffer + buffer_index + 2 * MAX_PIPE_PATH_LENGTH, notif_pipe, MAX_PIPE_PATH_LENGTH);
        pthread_mutex_unlock(&buffer_mutex);

        // Mudar o índice de acesso ao buffer
        pthread_mutex_lock(&buffer_index_mutex);
        buffer_index += 3 * MAX_PIPE_PATH_LENGTH;
        pthread_mutex_unlock(&buffer_index_mutex);

        // Post de informação para ler
        sem_post(&info_to_read);
    }

    // esperar pelas threads gestoras
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
        pthread_join(manager_threads[i], NULL);
    }

    // Fechar e apagar o pipe do server
    close(server_fd);
    if (unlink(server_path) == -1 && errno != ENOENT) {
        perror("Erro de unlink\n");
    }

    return NULL;
}

// Função principal
void* client_manager(void* args) {
    char *server_path = (char*) args;
    
    // Inicializar semáforos e mutex
    sem_init(&space_in_buffer, 0, MAX_SESSION_COUNT); // Buffer inicialmente vazio
    sem_init(&info_to_read, 0, 0); // Nenhuma informação disponível inicialmente
    pthread_mutex_init(&buffer_mutex, NULL);
    pthread_mutex_init(&buffer_index_mutex, NULL);

    // Criar a tarefa anfitriã
    if (pthread_create(&host_thread, NULL, host_task, (void*) server_path) != 0) {
        fprintf(stderr, "Failed to create host thread\n");
        exit(EXIT_FAILURE);
    }

    // esperar que a tarefa anfirtriã acabe
    pthread_join(host_thread, NULL);

    // Destruir semáforos e mutex
    sem_destroy(&space_in_buffer);
    sem_destroy(&info_to_read);
    pthread_mutex_destroy(&buffer_mutex);
    pthread_mutex_destroy(&buffer_index_mutex);

    return NULL;
}