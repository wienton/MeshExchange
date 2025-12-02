#ifndef SERVER_H
#define SERVER_H

#define DEFAULT_PORT 8124
#define DEFAULT_IP "127.0.0.1"

// error 
typedef enum {
    NET_SUCCESS,
    ERROR_NETWORK,
    ERROR_MEMORY,
    ERROR_SERVER,
    ERROR_FROM_CLIENT,
} error_t;

#endif