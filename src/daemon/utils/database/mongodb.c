#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mongoc/mongoc.h>
// header
#include "mongodb.h"
#include "logs/dblogs.h"
#include <ctype.h>

int ensure_docker_compose(void) {
    // Try to open for reading
    FILE* fp = fopen(filename_docker, "r");
    if (!fp) {
        // File doesn't exist — create it
        log_info("docker-compose.yml not found, creating default");
        goto write_default;
    }

    // Check if file is empty or only whitespace
    int ch;
    int has_content = 0;
    while ((ch = fgetc(fp)) != EOF) {
        if (!isspace(ch)) {
            has_content = 1;
            break;
        }
    }
    fclose(fp);

    if (!has_content) {
        log_info("docker-compose.yml is empty or whitespace-only, writing default");
        goto write_default;
    }

    log_debug("docker-compose.yml exists and has content");
    return 0;

write_default:
    fp = fopen(filename_docker, "w");
    if (!fp) {
        log_error("Failed to create docker-compose.yml");
        return -1;
    }
    if (fputs(default_docker_compose, fp) == EOF) {
        log_error("Failed to write default docker-compose.yml");
        fclose(fp);
        return -1;
    }
    fclose(fp);
    log_info("Wrote default docker-compose.yml");
    return 0;
}
// auto start docker container with mongo, start by open binary file 
int auto_start_docker(void) {
    FILE* fp = fopen(filename_docker, "r");
    if (!fp) {
        perror("error opening docker-compose.yml");
        log_error("Failed to open docker-compose.yml");
        return -1;
    }
    fclose(fp);

    if (system("docker-compose up -d") == -1) {
        perror("error starting docker-compose");
        log_error("Failed to start docker-compose; check Docker status and compose file");
        return -1;
    }

    log_info("Successfully started docker-compose from: 'docker-compose.yml'");

    fp = fopen(filename_docker, "r");
    if (fp) {
        char line[512];
        log_debug("Contents of docker-compose.yml:");
        while (fgets(line, sizeof(line), fp)) {
            // Remove trailing newline for clean logging
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
            }
            printf("%s\n", line);
        }
        fclose(fp);
  }
/*
    if (ensure_docker_compose() != 0) {
        return -1;
    }

    if (system("docker-compose up -d") == -1) {
        log_error("Failed to start docker-compose");
        return -1;
    }
*/
    log_info("Docker Compose started successfully");

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
        print_error("Auto stack docker container from docker-compose.yml\n");
        log_info("Start docker container from '%s'", filename_docker);
        goto start_container;
        connect_mongoc();
        mongoc_collection_destroy(collection);
        mongoc_client_destroy(client);
        return -1;
    }

start_container:
    auto_start_docker();

    
    // logs from file
    print_ok("\tSuccess ping database with admin\n");
    print_info("\tConfiguration connect: \n");
    print_green("Mongo URI: %s\n", MONGO_URI);
    print_green("MongoDatabase name: %s\n", MONGO_DATABASE_NAME);
    print_green("MongoCollection name: %s\n", MONGO_COLL_NAME);
    log_info(" Database module initialized.\n");
    log_info(" app.log success initialize, check log file for view full information");

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