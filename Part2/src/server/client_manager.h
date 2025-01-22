// @brief estrutura que contém as file descriptors dos pipes de um cliente
typedef struct {
    int req_fd;
    int resp_fd;
    int notif_fd;
} Client;

/// @brief Inicializa semáforos e mutexes e cria a host thread
/// @param server_path 
/// @return void*
void* client_manager(void *args);

/// @brief thread gestora para tratar de um cliente e processar os seus pedidos
void *manager_thread();

/// @brief thread anfitriã que lê pedidos de conexão de novos clientes
/// @param arg pipe do servidor
/// @return 
void *host_task(void* arg);

/// @brief Apaga todas as subscrições de um cliente e remove-o da lista de
/// clientes conectados
/// @param client Cliente a apagar
void delete_client(Client **client);

/// @brief Adiciona um cliente à lista de clientes
/// @param req_fd pipe de requests do cliente
/// @param resp_fd pipe de response do cliente
/// @param notif_fd pipe de notifications do cliente
Client* add_client(int req_fd, int resp_fd, int notif_fd);

/// @brief Fecha todos os pipes de um cliente
/// @param req_fd pipe de requests do cliente
/// @param resp_fd pipe de response do cliente
/// @param notif_fd pipe de notifications do cliente
void close_pipes(int req_fd, int resp_fd, int notif_fd);

/// @brief Desconecta todos os clientes e apaga todas as suas subscrições
/// quando o sinal SIGUSR1 é detetado
/// @param sig sinal a tratar
void sigusr1_handler(int sig);