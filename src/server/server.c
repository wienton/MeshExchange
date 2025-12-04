#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include "include/server.h"

// Worker function for each client
void* handle_client(void* arg) {
    int client_fd = *(int*)arg;
    free(arg); // free memory allocated for fd

    char buffer[1024];
    ssize_t n;

    printf("[+] New client connected\n");

    // Echo loop (example)
    while ((n = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        printf("Received: %s", buffer);
        send(client_fd, buffer, n, 0); // echo back
    }

    printf("[-] Client disconnected\n");
    close(client_fd);
    pthread_detach(pthread_self()); // auto-cleanup
    return NULL;
}

error_t start_server(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd <= 0) {
        perror("socket creation failed");
        return ERROR_SERVER;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        return ERROR_SERVER;
    }

    if (listen(sockfd, SOMAXCONN) < 0) {
        perror("listen failed");
        close(sockfd);
        return ERROR_SERVER;
    }

    printf("Server listening on port %d\n", port);
    return (error_t)sockfd; // return fd as success indicator
}

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        char* end;
        long val = strtol(argv[1], &end, 10);
        if (*end == '\0' && val > 0 && val <= 65535) {
            port = (int)val;
        } else {
            fprintf(stderr, "Invalid port, using default %d\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
    }

    int server_fd = start_server(port);
    if (server_fd <= 0) {
        return EXIT_FAILURE;
    }

    printf("Server ready. Waiting for connections...\n");

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd;

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept failed");
            break;
        }

        // Pass client_fd to new thread
        int* pclient_fd = malloc(sizeof(int));
        if (!pclient_fd) {
            perror("malloc failed");
            close(client_fd);
            continue;
        }
        *pclient_fd = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, pclient_fd) != 0) {
            perror("pthread_create failed");
            free(pclient_fd);
            close(client_fd);
        }
        // Note: thread is detached inside handle_client()
    }

    close(server_fd);
    return EXIT_SUCCESS;
}