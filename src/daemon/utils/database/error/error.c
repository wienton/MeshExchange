#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// header

#include "error.h"

// logs

#include "../logs/dblogs.h"


int main(void) {
    db_error_t db_err_t;

    switch(db_err_t) {

        case ERROR_CONNECT_DB:
            log_error("ERROR_CONNECT DATABASE! PLEASE CHECK NETWORK");
            break;
        
        case ERROR_NETWORK: 
            log_error("ERROR NETWORK STATE, PLEASE CHECK CONNECT");
            break;
        
        case ERROR_CONTAINER:
            log_error("Failed to start docker container, please check file");
            break;
        
        case ERROR_MEMORY:
            log_error("Error memory(segmentation fault)");
            break;
    
        case ERROR_WRITE_DB:
            log_error("error write info in database Mongo");
            break;
        
        default:
            log_error("Unknow error");
            break;
    
    }

}