#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "include/client.h"


error_cl_t connect_client(int *clientfd, struct sockaddr_in *server_addr) {
    // Создание клиентского сокета
    *clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*clientfd <= 0) {
        perror("socket creation failed");
        return ERROR_NETWORK;
    }

    // Настройка адреса сервера
    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(DEFAULT_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr->sin_addr) <= 0) {
        perror("invalid server address");
        close(*clientfd);
        return ERROR_NETWORK;
    }

    // Подключение к серверу
    if (connect(*clientfd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("connection failed");
        close(*clientfd);
        return ERROR_NETWORK;
    }

    printf("Connected to server at %s:%d\n", SERVER_IP, DEFAULT_PORT);
    return ERROR_NETWORK;
}

int main(void) {
    int sockfd;
    struct sockaddr_in server_addr;

    if (connect_client(&sockfd, &server_addr) != CLIENT_SUCCESS) {
        fprintf(stderr, "Failed to connect to server\n");
        return EXIT_FAILURE;
    }

    printf("Client is connected and ready to send/receive data.\n");


    close(sockfd);
    return EXIT_SUCCESS;
}