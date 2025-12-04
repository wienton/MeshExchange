#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "include/client.h"

error_cl_t connect_client(int *clientfd, struct sockaddr_in *server_addr, const char *ip, int port) {
    *clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*clientfd <= 0) {
        perror("socket creation failed");
        return ERROR_NETWORK;
    }

    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr->sin_addr) <= 0) {
        perror("invalid server IP address");
        close(*clientfd);
        return ERROR_NETWORK;
    }

    if (connect(*clientfd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("connection failed");
        close(*clientfd);
        return ERROR_NETWORK;
    }

    printf("Connected to server at %s:%d\n", ip, port);
    return CLIENT_SUCCESS;  // <-- was missing!
}

int main(int argc, char *argv[]) {
    const char *ip = SERVER_IP;
    int port = DEFAULT_PORT;

    // Parse IP from argv[1], port from argv[2]
    if (argc > 1) {
        ip = argv[1];
        if (argc > 2) {
            char *end;
            long val = strtol(argv[2], &end, 10);
            if (*end == '\0' && val > 0 && val <= 65535) {
                port = (int)val;
            } else {
                fprintf(stderr, "Invalid port: %s. Using default %d.\n", argv[2], DEFAULT_PORT);
                port = DEFAULT_PORT;
            }
        }
    }

    int sockfd;
    struct sockaddr_in server_addr;

    if (connect_client(&sockfd, &server_addr, ip, port) != CLIENT_SUCCESS) {
        fprintf(stderr, "Failed to connect to server\n");
        return EXIT_FAILURE;
    }

    printf("Client is connected and ready to send/receive data.\n");

    close(sockfd);
    return EXIT_SUCCESS;
}