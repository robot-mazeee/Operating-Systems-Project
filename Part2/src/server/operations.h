#ifndef KVS_OPERATIONS_H
#define KVS_OPERATIONS_H

#include <stddef.h>

/// Initializes the KVS state.
/// @return 0 if the KVS state was initialized successfully, 1 otherwise.
int kvs_init();

/// Destroys the KVS state.
/// @return 0 if the KVS state was terminated successfully, 1 otherwise.
int kvs_terminate();

/// @brief Sorts (key, value) pairs by key alphabetical order
/// @param num_pairs Number of pairs to sort
/// @param keys Array of keys' strings
/// @param values Array of values' strings
void sort_keys_values(size_t num_pairs, char keys[][MAX_STRING_SIZE], char values[][MAX_STRING_SIZE]);

/// Writes a key value pair to the KVS. If key already exists it is updated.
/// @param num_pairs Number of pairs being written.
/// @param keys Array of keys' strings.
/// @param values Array of values' strings.
/// @return 0 if the pairs were written successfully, 1 otherwise.
int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE], char values[][MAX_STRING_SIZE]);

/// @brief Sorts keys by alphabetical order
/// @param num_pairs Number of keys
/// @param keys Array of keys' strings
void sort_keys(size_t num_pairs, char keys[][MAX_STRING_SIZE]);

/// Reads values from the KVS.
/// @param num_pairs Number of pairs to read.
/// @param keys Array of keys' strings.
/// @param file_out File descriptor to write the (successful) output.
/// @return 0 if the key reading, 1 otherwise.
int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int file_out);

/// Deletes key value pairs from the KVS.
/// @param num_pairs Number of pairs to read.
/// @param keys Array of keys' strings.
/// @param file_out File descriptor to write the (unsuccessful) output.
/// @return 0 if the pairs were deleted successfully, 1 otherwise.
int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int file_out);

/// Writes the state of the KVS.
/// @param file_out File descriptor to write the output.
void kvs_show(int file_out);

/// Creates a backup of the KVS state and stores it in the correspondent
/// backup file.
/// @param pathname Path for the file that requested the backup
/// @param max_backups Maximum simultaneous backups allowed
/// @param simultaneous_backups Current simultaneous backups 
/// @param backup_num Number of the current backup
/// @return 0 if the backup was successful, 1 otherwise.
int kvs_backup(char pathname[], int max_backups, int *simultaneous_backups, int backup_num);

/// Waits for a given amount of time.
/// @param file_out File descriptor to write the output.
/// @param delay_us Delay in milliseconds.
void kvs_wait(int file_out, unsigned int delay_ms);

/// @brief Adds a key to a client's subscriptions
/// @param key key to subscribe
/// @param notif_fd file descriptor for the client's notifications pipe
/// @return 1 if the operation is successful and 0 otherwise
int subscribe_key(char *key, int notif_fd);

/// @brief Removes a key from a client's subscriptions
/// @param key key to unsubscribe
/// @param notif_fd file descriptor for the client's notifications pipe
/// @return 0 if the operation is successful and 1 otherwise
int unsubscribe_key(char *key, int notif_fd);

/// @brief Deletes all subscriptions from one client
/// @param notif_fd file descriptor for the client's notifications pipe
void delete_all_subs(int notif_fd);

#endif  // KVS_OPERATIONS_H
