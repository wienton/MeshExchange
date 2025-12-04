#ifndef DB_ERROR_H
#define DB_ERROR_H


typedef enum {

    ERROR_CONNECT_DB,
    ERROR_NETWORK,
    ERROR_CONTAINER,
    ERROR_MEMORY,
    ERROR_WRITE_DB,

} db_error_t;



#endif