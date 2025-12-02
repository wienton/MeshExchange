#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>

// header
#include "include/server.h"

error_t connect_server(int *sockfd, struct sockaddr_in *server_addr) {
    // create socket
    *sockfd = socket(
         AF_INET,
         SOCK_STREAM, 
         0
    );
    
    if (*sockfd <= 0) {
        perror("error socket create");
        return ERROR_SERVER;
    }

    // settings struct for address server
    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(DEFAULT_PORT);          
    server_addr->sin_addr.s_addr = htonl(INADDR_ANY);    

    // bind, socket in addreess
    if (bind(*sockfd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("error bind");
        close(*sockfd);
        return ERROR_SERVER;
    }

    // listen
    if (listen(*sockfd, SOMAXCONN) < 0) {
        perror("error listen");
        close(*sockfd);
        return ERROR_SERVER;
    }
    // success
    printf("Server is listening on port %d\n", DEFAULT_PORT);
    return NET_SUCCESS;
}  

int main(void) {
    int sockfd;
    struct sockaddr_in server_addr;

    if (connect_server(&sockfd, &server_addr) != NET_SUCCESS) {
        fprintf(stderr, "Failed to start server\n");
        return EXIT_FAILURE;
    }


    printf("Server ready. Waiting for connections...\n");

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept failed");
    } else {
        printf("Client connected!\n");
        close(client_fd);
    }

    close(sockfd);
    return EXIT_SUCCESS;
}