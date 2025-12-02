#ifndef CLIENT_H
#define CLIENT_H


#define DEFAULT_PORT 8080
#define SERVER_IP "127.0.0.1" 

typedef enum {

    CLIENT_SUCCESS,
    ERROR_MEMORY,
    ERROR_NETWORK,
    ERROR_CONNECT,
    ERROR_THREAD,

} error_cl_t;

#endif 