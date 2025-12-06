/**
    * @brief mongodb module
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mongoc/mongoc.h>
// header
#include "mongodb.h"
#include "logs/dblogs.h"
#include <ctype.h>

// libflux header-only
#include "../../../../lib/libflux/libflux.h"

// error 
#include "error/error.h"


db_error_t ensure_docker_compose(void) {
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
    sleep(3);
    log_info("Successfully started docker-compose from: 'docker-compose.yml'");

    fp = fopen(filename_docker, "r");
    if (fp) {
        goto content_docker;
   }
content_docker:
    char line[512];
    log_debug("Contents of docker-compose.yml:");
    sleep(3);
    while (fgets(line, sizeof(line), fp)) {
        // Remove trailing newline for clean logging
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        printf("%s\n", line);
    }
    print_info("finished view content docker-compose.yml\n\n");
    fclose(fp);
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

    sleep(3);
    // logs from file
    print_ok("\tSuccess ping database with admin\n");
    
    print_info("\tConfiguration connect: \n");
    print_green("Mongo URI: %s\n", MONGO_URI);
    print_green("MongoDatabase name: %s\n", MONGO_DATABASE_NAME);
 
    print_green("MongoCollection name: %s\n", MONGO_COLL_NAME);
    print_info(" Database module initialized.\n");
    print_info(" app.log success initialize, check log file for view full information\n");

   
    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);


    return 0;
}

int view_status_database() {

    mongoc_client_t *client = mongoc_client_new(MONGO_URI);
    if (!client) {
        fprintf(stderr, "error: failed to connect to MongoDB\n");
        exit(EXIT_FAILURE);
    }

    mongoc_collection_t *collection = mongoc_client_get_collection(client, MONGO_DATABASE_NAME, MONGO_COLL_NAME);
    if (!collection) {
        fprintf(stderr, "error: cannot access collection %s.%s\n", MONGO_DATABASE_NAME, MONGO_COLL_NAME);
        mongoc_client_destroy(client);
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 3; ++i) {print_ok("%d module initialize...\n", i); sleep(3);}

    sleep(2);

    log_info("View MongoDatabase 'exchange' success started on thread \n");

    for(;;) {goto mongo_viewer;}

mongo_viewer:
    printf("[MONITOR] Monitor MongoDatabase Success Started [%s.%s]", MONGO_DATABASE_NAME, MONGO_COLL_NAME);

    while (1) {
        bson_t query = BSON_INITIALIZER;
        mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(collection, &query, NULL, NULL);
	
        bool found = false;
        const bson_t *doc;
        while (mongoc_cursor_next(cursor, &doc)) {
            char *json = bson_as_relaxed_extended_json(doc, NULL);
            if (json) {
                printf("%s\n", json);
                bson_free(json);
                found = true;
            }
        }

        bson_error_t error;
        if (mongoc_cursor_error(cursor, &error)) {
            fprintf(stderr, "cursor error: %s\n", error.message);
        }

        if (!found) {
            log_debug("right now records not have");
        }

        mongoc_cursor_destroy(cursor);
        bson_destroy(&query);

        sleep(POLL_INTERVAL_SEC);
    }


}


STATUS_CONNECT init_module() {
   const char* moduleArray[] = {filename_docker, "mongodb", "redis"};
   size_t numModules = sizeof(moduleArray) / sizeof(moduleArray[0]);

    print_blue("\n\n\tSuccess init module for database MONGO!\n\n");

    print_blue("\n\n\t Module Info: \n\n");


    for(int i = 0; i < numModules; i++) {
        printf("module[%d] %s\n", i, moduleArray[i]);
        sleep(1);
    }

}

char* execute_command(const char* cmd) {
    FILE* fp = popen(cmd, "r");
    if (fp == NULL) {
        perror("Failed to run command");
        return NULL;
    }

    char buffer[1024];
    char* result = NULL;
    size_t total_size = 0;

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        size_t current_len = strlen(buffer);
        result = (char*)realloc(result, total_size + current_len + 1);
        if (result == NULL) {
            perror("Memory allocation failed");
            pclose(fp);
            return NULL;
        }
        strcpy(result + total_size, buffer);
        total_size += current_len;
    }
    pclose(fp);
    return result;
}

int check_docker_running() {
    printf("Checking system...\n");

    // 1. Проверка операционной системы
    char* os_name = NULL;
    char command[MAX_CMD_LEN];

    // Попытка определить OS, начиная с самых распространенных
    if (access("/etc/os-release", F_OK) == 0) { // Linux
        sprintf(command, "grep -E '^ID=' /etc/os-release | cut -d'=' -f2 | tr -d '\"'");
        os_name = execute_command(command);
        if (os_name) {
            // Удаляем возможные символы новой строки и пробелы с конца
            size_t len = strlen(os_name);
            while (len > 0 && (os_name[len-1] == '\n' || isspace(os_name[len-1]))) {
                os_name[--len] = '\0';
            }
            printf("Detected OS: Linux (%s)\n", os_name);
            free(os_name); // Освобождаем память после использования
            os_name = NULL;
        } else {
            printf("Detected OS: Linux (could not determine specific distribution)\n");
        }
    } else if (access("/System/Library/CoreServices/SystemVersion.plist", F_OK) == 0) { // macOS
        printf("Detected OS: macOS\n");
    } else if (access("C:\\Windows\\System32\\dllhost.exe", F_OK) == 0) { // Простой способ для Windows
        printf("Detected OS: Windows\n");
    } else {
        printf("Could not determine OS. Proceeding with generic checks.\n");
    }

    // 2. Проверка, установлен ли Docker
    printf("Checking if Docker is installed...\n");
    if (system("which docker > /dev/null 2>&1") != 0) {
        printf("Docker is not installed.\n");
        char choice;
        printf("Do you want to install Docker? (y/n): ");
        if (scanf(" %c", &choice) != 1) { // Пробел перед %c, чтобы съесть предыдущие символы новой строки
            printf("Invalid input. Aborting.\n");
            return -1; // Возвращаем ошибку, если ввод некорректный
        }

        if (choice == 'y' || choice == 'Y') {
            printf("Please refer to the official Docker documentation for installation instructions for your OS.\n");
            printf("For Linux: https://docs.docker.com/engine/install/\n");
            printf("For macOS: https://docs.docker.com/desktop/install/mac-install/\n");
            printf("For Windows: https://docs.docker.com/desktop/install/windows-install/\n");
            return 0; // Докер не установлен, но пользователь хочет установить
        } else {
            printf("Docker installation skipped by user.\n");
            return 0; // Докер не установлен, и пользователь отказался устанавливать
        }
    }

    // 3. Если Docker установлен, проверяем, запущен ли он
    printf("Docker is installed. Checking if it's running...\n");

    // Используем `docker info` или `docker ps`, чтобы понять, запущен ли демон
    // `docker info` более надежен, так как `docker ps` может быть пуст, но демон работает
    char* docker_info_output = execute_command("docker info");
    if (docker_info_output == NULL) {
        printf("Failed to get Docker info. Possible issue with Docker daemon.\n");
        return -1; // Ошибка выполнения команды
    }

    // Ищем ключевую фразу, которая указывает на то, что демон не запущен
    if (strstr(docker_info_output, "Cannot connect to the Docker daemon") != NULL ||
        strstr(docker_info_output, "Client: Error during connect") != NULL) {
        printf("Docker daemon is not running.\n");
        free(docker_info_output); // Освобождаем память
        return 0; // Докер не запущен
    }

    free(docker_info_output); // Освобождаем память
    printf("Docker daemon is running.\n");
    return 1; // Докер запущен
}
// main
int main(void) {

    int status = check_docker_running();

    if (status == 1) {
        printf("\nResult: Docker is running.\n");
    } else if (status == 0) {
        printf("\nResult: Docker is not running or not installed, or user opted not to install.\n");
    } else {
        printf("\nResult: An error occurred.\n");
    }

    init_module();

    db_error_t db_err_t;

    printf("db_err_t success connect");

    // init log function 
    if(dblog_init("app.log")) {fprintf(stderr, "error init logger");}


    print_ok("\tsuccess init logger\n");
    // init database from function
    if(connect_mongoc()) {fprintf(stderr, "error init mongo from main, check your function or logs\n");}


    if(view_status_database()) {fprintf(stderr, "error view monitor initialize from main"); return -1;}

    dblog_close();
    
}
