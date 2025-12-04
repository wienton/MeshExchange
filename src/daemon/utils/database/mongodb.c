#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mongoc/mongoc.h>
// header
#include "mongodb.h"
#include "logs/dblogs.h"
// auto start docker container with mongo, start by open binary file 
int auto_start_docker() {
    FILE* fp = fopen(filename_docker, "r");
    if (!fp) {
        perror("error opening docker-compose.yml");
        return -1;
    }
    fclose(fp);

    if (system("docker-compose up -d") == -1) {
        perror("error starting docker-compose");
        return -1;
    }

    return 0;
}
// connect mongo database by params 1) Mongo Uri, 2) MongoDb Name, 3) Mongo Collection Name
int connect_mongoc(void){
    mongoc_client_t *client;
    mongoc_collection_t *collection;
    bson_error_t error;

    // initialization mongo 
    mongoc_init();

    client = mongoc_client_new(MONGO_URI);
    if (!client) {
        fprintf(stderr, "Failed to create MongoDB client.\n");
        return -1;
    }

    collection = mongoc_client_get_collection(client, MONGO_DATABASE_NAME, MONGO_COLL_NAME);

    // check connect, simple ping
    bson_t *command = BCON_NEW("ping", BCON_INT32(1));
    bson_t reply;
    bool success = mongoc_client_command_simple(client, "admin", command, NULL, &reply, &error);

    bson_destroy(&reply);
    bson_destroy(command);

    if (!success) {
        log_error("Mongo Ping failed!\n");
        auto_start_docker();
        connect_mongoc();
        mongoc_collection_destroy(collection);
        mongoc_client_destroy(client);
        return -1;
    }

    
    // logs from file
    print_ok("\tSuccess ping database with admin\n");
    print_info("\tConfiguration connect: \n");
    print_green("Mongo URI: %s\n", MONGO_URI);
    print_green("MongoDatabase name: %s\n", MONGO_DATABASE_NAME);
    print_green("MongoCollection name: %s\n", MONGO_COLL_NAME);
    log_info(" Database module initialized.");

    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);

    return 0;
}

int main(void) {
    // init log function 
    if(dblog_init("app.log")) {fprintf(stderr, "error init logger");}


    print_ok("\tsuccess init logger\n");
    // init database from function
    if(connect_mongoc()) {fprintf(stderr, "error init mongo from main, check your function or logs\n");}


    dblog_close();
    
}