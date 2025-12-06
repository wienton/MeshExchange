#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h> // For mkdir
#include <fcntl.h>    // For file operations

#define BUFFER_SIZE 4096    // Increased buffer size for file transfer efficiency
#define SMALL_BUF_SIZE 256  // For regular messages and log_buf
#define MAX_PENDING_CONN 5
#define PATH_MAX 1024

// Define the directory where uploaded files will be saved
#define UPLOAD_DIR "uploaded_files"

// Global socket descriptors for cleaning up on exit
int server_socket_fd = -1;
int client_socket_fd_global = -1; // To track the currently active client socket

// Function to print timestamped messages to stdout
void log_info(const char *message) {
    time_t now = time(NULL);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stdout, "[%s] %s\n", time_buf, message);
    fflush(stdout);
}

// Function to print timestamped errors to stderr
void log_error(const char *message) {
    time_t now = time(NULL);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stderr, "[%s] Error: %s\n", time_buf, message);
    fflush(stderr);
}

// Signal handler for graceful shutdown (e.g., Ctrl+C)
void handle_sigint(int sig) {
    log_info("Received SIGINT. Shutting down server...");
    if (client_socket_fd_global != -1) {
        close(client_socket_fd_global);
        client_socket_fd_global = -1;
    }
    if (server_socket_fd != -1) {
        close(server_socket_fd);
        server_socket_fd = -1;
    }
    exit(EXIT_SUCCESS);
}

// Helper to send a simple response to the client
void send_response(int sock_fd, const char* response) {
    if (write(sock_fd, response, strlen(response)) == -1) {
        log_error("Failed to send response to client.");
        perror("write");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        log_error("Usage: <server_port>");
        exit(EXIT_FAILURE);
    }

    int server_port = atoi(argv[1]);
    if (server_port <= 0 || server_port > 65535) {
        log_error("Invalid port number. Must be between 1 and 65535.");
        exit(EXIT_FAILURE);
        }

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char message_buffer[BUFFER_SIZE]; // Use BUFFER_SIZE for network ops
    char log_buf[SMALL_BUF_SIZE];     // Use SMALL_BUF_SIZE for logs

    snprintf(log_buf, sizeof(log_buf), "Starting server on port %d", server_port);
    log_info(log_buf);

    signal(SIGINT, handle_sigint);

    // Create UPLOAD_DIR if it doesn't exist
    if (mkdir(UPLOAD_DIR, 0755) == -1 && errno != EEXIST) {
        snprintf(log_buf, sizeof(log_buf), "Failed to create upload directory %s: %s", UPLOAD_DIR, strerror(errno));
        log_error(log_buf);
        exit(EXIT_FAILURE);
    }
    snprintf(log_buf, sizeof(log_buf), "Upload directory set to: %s", UPLOAD_DIR);
    log_info(log_buf);

    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd == -1) {
        log_error("Socket creation failed.");
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int optval = 1;
    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        log_error("setsockopt(SO_REUSEADDR) failed (non-critical).");
        perror("setsockopt");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(server_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        log_error("Socket binding failed.");
        perror("bind");
        close(server_socket_fd);
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_socket_fd, MAX_PENDING_CONN) == -1) {
        log_error("Could not listen on socket.");
        perror("listen");
        close(server_socket_fd);
        exit(EXIT_FAILURE);
    }

    snprintf(log_buf, sizeof(log_buf), "Server listening on port %d. Waiting for connections...", server_port);
    log_info(log_buf);

    while (1) {
        log_info("Waiting for a client connection request...");
        client_socket_fd_global = accept(server_socket_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket_fd_global == -1) {
            if (errno == EINTR) { 
                log_info("Accept interrupted by signal. Exiting accept loop.");
                break;
            }
            log_error("Failed to accept client connection.");
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        char response_char;
        fprintf(stdout, "Incoming connection from %s:%d. Accept? (y/n): ", client_ip, client_port);
        fflush(stdout);

        while (scanf(" %c", &response_char) != 1 || (response_char != 'y' && response_char != 'Y' && response_char != 'n' && response_char != 'N')) {
            fprintf(stdout, "Invalid input. Please enter 'y' or 'n': ");
            fflush(stdout);
            while (getchar() != '\n');
        }
        while (getchar() != '\n'); 

        if (response_char == 'y' || response_char == 'Y') {
            snprintf(log_buf, sizeof(log_buf), "Accepted connection from %s:%d. Starting session.", client_ip, client_port);
            log_info(log_buf);
            send_response(client_socket_fd_global, "Connection accepted. You can send messages now (or 'upload <file>').");

            while (1) {
                memset(message_buffer, 0, BUFFER_SIZE);
                ssize_t bytes_received = recv(client_socket_fd_global, message_buffer, BUFFER_SIZE - 1, 0); // Using recv

                if (bytes_received == -1) {
                    if (errno == EINTR) {
                        log_info("Read from client interrupted by signal. Ending session.");
                    } else {
                        log_error("Failed to read from client socket during session.");
                        perror("read");
                    }
                    break;
                }else if (bytes_received == 0) {
                    log_info("Client disconnected.");
                    break;
                } else {
                    message_buffer[bytes_received] = '\0';
                    
                    // --- Command Parsing: Check for UPLOAD command ---
                    if (strncmp(message_buffer, "UPLOAD ", 7) == 0) {
                        char filename[256];
                        long filesize;
                        char *ptr = message_buffer + 7; // Skip "UPLOAD "

                        // Parse filename and filesize
                        if (sscanf(ptr, "%255s %ld", filename, &filesize) == 2) {
                            snprintf(log_buf, sizeof(log_buf), "Client requested UPLOAD: file '%s', size %ld bytes.", filename, filesize);
                            log_info(log_buf);

                            char full_path[PATH_MAX];
                            snprintf(full_path, sizeof(full_path), "%s/%s", UPLOAD_DIR, filename);

                            // Open file for writing
                            int file_fd = open(full_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                            if (file_fd == -1) {
                                snprintf(log_buf, sizeof(log_buf), "Failed to open file '%s' for writing: %s", full_path, strerror(errno));
                                log_error(log_buf);
                                send_response(client_socket_fd_global, "ERROR: Could not create file on server.");
                            } else {
                                send_response(client_socket_fd_global, "READY_FOR_FILE"); // Tell client to start sending file data
                                
                                long total_received = 0;
                                ssize_t segment_bytes;
                                // Loop to receive file data
                                while (total_received < filesize) {
                                    size_t to_read = (filesize - total_received > BUFFER_SIZE) ? BUFFER_SIZE : (filesize - total_received);
                                    
                                    segment_bytes = recv(client_socket_fd_global, message_buffer, to_read, 0);
                                    if (segment_bytes == -1) {
                                        log_error("Error receiving file data from client.");
                                        perror("recv");
                                        break;
                                    }
                                    if (segment_bytes == 0) {
                                        log_info("Client disconnected during file transfer.");
                                        break;
                                    }

                                    if (write(file_fd, message_buffer, segment_bytes) == -1) {
                                        log_error("Error writing file data to disk.");
                                        perror("write");
                                        break;
                                    }
                                    total_received += segment_bytes;
                                }
                                close(file_fd);

                                if (total_received == filesize) {
                                    snprintf(log_buf, sizeof(log_buf), "File '%s' (%ld bytes) successfully received and saved to '%s'.", filename, filesize, full_path);
                                    log_info(log_buf);
                                    send_response(client_socket_fd_global, "UPLOAD_SUCCESS");
                                } else {
                                    snprintf(log_buf, sizeof(log_buf), "Incomplete file transfer for '%s'. Expected %ld, received %ld.", filename, filesize, total_received);
                                    log_error(log_buf);
                                    send_response(client_socket_fd_global, "UPLOAD_FAILED: Incomplete transfer.");
                                    remove(full_path); // Clean up incomplete file
                                }
                            }
                        } else {
                            log_error("Invalid UPLOAD command format received.");
                            send_response(client_socket_fd_global, "ERROR: Invalid UPLOAD command format.");
                        }
                    } else { // It's a regular message
                        snprintf(log_buf, sizeof(log_buf), "Received from %s:%d: \"%s\"", client_ip, client_port, message_buffer);
                        log_info(log_buf);
                        send_response(client_socket_fd_global, "MESSAGE_RECEIVED"); // Acknowledge regular message
                    }
                }
            } // End of client session loop

            log_info("Client session ended. Closing client socket.");
            close(client_socket_fd_global);
            client_socket_fd_global = -1;
        } else {
            snprintf(log_buf, sizeof(log_buf), "Connection from %s:%d rejected.", client_ip, client_port);
            log_info(log_buf);
            send_response(client_socket_fd_global, "Connection rejected by server.");
            close(client_socket_fd_global);
            client_socket_fd_global = -1;
        }
    } // End of main server loop

    log_info("Server shutdown complete.");
    if (server_socket_fd != -1) {
        close(server_socket_fd);
    }
    return EXIT_SUCCESS;
}