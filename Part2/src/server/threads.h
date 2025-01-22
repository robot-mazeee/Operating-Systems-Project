#ifndef KVS_THREADS_H
#define KVS_THREADS_H

#include "constants.h"

typedef struct {
    char pathname[PATH_MAX];
    int max_backups;
} ThreadArgs;

#endif