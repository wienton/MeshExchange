#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h> // For stat
#include <fcntl.h>    // For file operations

#define BUFFER_SIZE 4096       // Increased buffer size for file transfer efficiency
#define SMALL_BUF_SIZE 256     // For regular messages and log_buf
#define MAX_RETRY_ATTEMPTS 10
#define RETRY_DELAY_SEC 3

// Function to print timestamped messages
void log_info(const char *message) {
    time_t now = time(NULL);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stdout, "[%s] %s\n", time_buf, message);
    fflush(stdout);
}

// Function to print timestamped errors
void log_error(const char *message) {
    time_t now = time(NULL);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stderr, "[%s] Error: %s\n", time_buf, message);
    fflush(stderr);
}

// Helper to send a message and wait for a single line response
char* send_command_and_get_response(int sock_fd, const char* command, char* response_buffer, size_t buffer_size) {
    if (send(sock_fd, command, strlen(command), 0) == -1) {
        log_error("Failed to send command to server.");
        perror("send");
        return NULL;
    }
    memset(response_buffer, 0, buffer_size);
    ssize_t bytes_read = recv(sock_fd, response_buffer, buffer_size - 1, 0);
    if (bytes_read <= 0) {
        if (bytes_read == -1) {
            log_error("Error receiving response from server.");
            perror("recv");
        } else {
            log_info("Server disconnected while waiting for response.");
        }
        return NULL;
    }
    response_buffer[bytes_read] = '\0';
    // Remove newline if it's there
    response_buffer[strcspn(response_buffer, "\n")] = 0;
    return response_buffer;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        log_error("Usage: <server_ip> <server_port>");
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    if (server_port <= 0 || server_port > 65535) {
        log_error("Invalid port number. Must be between 1 and 65535.");
        exit(EXIT_FAILURE);
    }

    int client_socket = -1;
    struct sockaddr_in server_addr;
    char message_buffer[BUFFER_SIZE];     // For sending/receiving actual data
    char input_buffer[SMALL_BUF_SIZE];    // For user input
    char log_buf[SMALL_BUF_SIZE];         // For simple logs
    char response_buf[SMALL_BUF_SIZE];    // For server responses
    int attempt = 0;
    
    snprintf(log_buf, sizeof(log_buf), "Attempting to connect to %s:%d", server_ip, server_port);
    log_info(log_buf);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        log_error("Invalid server IP address or address not supported.");
        exit(EXIT_FAILURE);
    }

    // Connection loop with retries
    while (attempt < MAX_RETRY_ATTEMPTS) {
        attempt++;
        snprintf(log_buf, sizeof(log_buf), "Connection attempt %d/%d...", attempt, MAX_RETRY_ATTEMPTS);
        log_info(log_buf);

        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1) {
            log_error("Socket creation failed.");
            sleep(RETRY_DELAY_SEC);
            continue;
        }

        if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
            break; // Successfully connected
        } else {
            char err_msg[SMALL_BUF_SIZE];
            snprintf(err_msg, sizeof(err_msg), "Connection failed (errno %d): %s", errno, strerror(errno));
            log_error(err_msg);
            close(client_socket);
            client_socket = -1;
            
            if (attempt < MAX_RETRY_ATTEMPTS) {
                snprintf(log_buf, sizeof(log_buf), "Retrying in %d seconds...", RETRY_DELAY_SEC);
                log_info(log_buf);
                sleep(RETRY_DELAY_SEC);
            }
        }
    }

    if (client_socket == -1) {
        log_error("Failed to connect to server after multiple attempts. Exiting.");
        exit(EXIT_FAILURE);
    }

    snprintf(log_buf, sizeof(log_buf), "Successfully connected to %s:%d. Waiting for server acceptance...", server_ip, server_port);
    log_info(log_buf);

    // Receive initial response from server (acceptance/rejection)
    memset(response_buf, 0, SMALL_BUF_SIZE);
    ssize_t bytes_received = recv(client_socket, response_buf, SMALL_BUF_SIZE - 1, 0);

    if (bytes_received == -1) {
        log_error("Error receiving initial response from server.");
        perror("recv");
        close(client_socket);
        exit(EXIT_FAILURE);
    } else if (bytes_received == 0) {
        log_info("Server closed connection unexpectedly during acceptance phase.");
        close(client_socket);
        exit(EXIT_FAILURE);
    } else {
        response_buf[bytes_received] = '\0';
        response_buf[strcspn(response_buf, "\n")] = 0; // Remove newline if any
        snprintf(log_buf, sizeof(log_buf), "Server response: \"%s\"", response_buf);
        log_info(log_buf);

        if (strstr(response_buf, "rejected") != NULL) {
            log_info("Connection rejected by server. Exiting.");
            close(client_socket);
            exit(EXIT_SUCCESS);
        }
    }
    
    log_info("You are now connected. Enter messages to send (or 'upload <file>', 'exit' to quit):");
    
    // Main client session loop
    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(input_buffer, SMALL_BUF_SIZE, stdin) == NULL) {
            log_error("Error reading input. Exiting session.");
            break;
        }
        input_buffer[strcspn(input_buffer, "\n")] = 0; // Remove newline

        if (strcmp(input_buffer, "exit") == 0) {
            log_info("Exiting session.");
            break;
        }

        if (strlen(input_buffer) == 0) { // Don't send empty lines
            continue;
        }

        // --- Command Parsing: Check for UPLOAD command ---
        if (strncmp(input_buffer, "upload ", 7) == 0) {
            char *filename_str = input_buffer + 7; // Skip "upload "
            if (strlen(filename_str) == 0) {
                log_error("Usage: upload <filename>");
                continue;
            }

            // Get file size and check if file exists
            struct stat file_stat;
            if (stat(filename_str, &file_stat) == -1) {snprintf(log_buf, sizeof(log_buf), "Error: File '%s' not found or inaccessible: %s", filename_str, strerror(errno));
                log_error(log_buf);
                continue;
            }
            if (!S_ISREG(file_stat.st_mode)) {
                snprintf(log_buf, sizeof(log_buf), "Error: '%s' is not a regular file.", filename_str);
                log_error(log_buf);
                continue;
            }
            long long filesize = file_stat.st_size;

            snprintf(log_buf, sizeof(log_buf), "Preparing to upload '%s' (%lld bytes).", filename_str, filesize);
            log_info(log_buf);

            // 1. Send UPLOAD command header to server
            char upload_cmd[SMALL_BUF_SIZE * 2]; // Enough for "UPLOAD <filename> <filesize>\n"
            snprintf(upload_cmd, sizeof(upload_cmd), "UPLOAD %s %lld", filename_str, filesize);
            
            char* server_ack_response = send_command_and_get_response(client_socket, upload_cmd, response_buf, sizeof(response_buf));

            if (server_ack_response == NULL) {
                log_error("Server did not respond to UPLOAD command. Connection likely lost.");
                break;
            }
            if (strcmp(server_ack_response, "READY_FOR_FILE") == 0) {
                log_info("Server ready for file data. Sending file...");
                
                // 2. Open file for reading
                int file_fd = open(filename_str, O_RDONLY);
                if (file_fd == -1) {
                    snprintf(log_buf, sizeof(log_buf), "Failed to open local file '%s' for reading: %s", filename_str, strerror(errno));
                    log_error(log_buf);
                    continue;
                }

                long long total_sent = 0;
                ssize_t bytes_read_from_file;
                ssize_t bytes_sent_to_server;

                // 3. Send file data in chunks
                while (total_sent < filesize) {
                    bytes_read_from_file = read(file_fd, message_buffer, BUFFER_SIZE);
                    if (bytes_read_from_file == -1) {
                        log_error("Error reading from local file.");
                        perror("read");
                        break;
                    }
                    if (bytes_read_from_file == 0) { // EOF
                        log_error("EOF reached unexpectedly while sending file (file size mismatch?).");
                        break;
                    }

                    bytes_sent_to_server = send(client_socket, message_buffer, bytes_read_from_file, 0);
                    if (bytes_sent_to_server == -1) {
                        log_error("Error sending file data to server.");
                        perror("send");
                        break;
                    }
                    total_sent += bytes_sent_to_server;
                }
                close(file_fd);

                if (total_sent == filesize) {
                    log_info("File data sent completely. Waiting for server confirmation...");
                    char* upload_final_response = send_command_and_get_response(client_socket, "FILE_DATA_SENT", response_buf, sizeof(response_buf));
                    if (upload_final_response == NULL) {
                         log_error("Server did not confirm upload. Connection lost?");
                         break;
                    }
                    if (strcmp(upload_final_response, "UPLOAD_SUCCESS") == 0) {
                        log_info("File upload successful!");
                    } else {
                        snprintf(log_buf, sizeof(log_buf), "Server reported upload failed: %s", upload_final_response);
                        log_error(log_buf);
                    }
                } else {
                    snprintf(log_buf, sizeof(log_buf), "Incomplete file data sent. Expected %lld, sent %lld.", filesize, total_sent);
                    log_error(log_buf);
                    // Server might eventually time out or report error
                }

            } else {
                snprintf(log_buf, sizeof(log_buf), "Server rejected upload request: %s", server_ack_response);
                log_error(log_buf);
            }

        } else { // It's a regular message
            if (send(client_socket, input_buffer, strlen(input_buffer), 0) == -1) {
                log_error("Message send failed. Server likely disconnected.");
                perror("send");
                break;
            }
            snprintf(log_buf, sizeof(log_buf), "Sent \"%s\"", input_buffer);
            log_info(log_buf);

            // Wait for basic acknowledgment for regular message
            char* msg_ack_response = send_command_and_get_response(client_socket, "ACK_REQUEST", response_buf, sizeof(response_buf));
             if (msg_ack_response == NULL) {
                 log_error("Server did not respond to message. Connection lost?");
                 break;
            }
            // log_info(msg_ack_response); // If you want to see the ACK
            if (strcmp(msg_ack_response, "MESSAGE_RECEIVED") != 0) {
                 log_error("Unexpected server response after sending message.");
            }
        }
    } // End of main client session loop

    log_info("Closing connection.");
    close(client_socket);

    return EXIT_SUCCESS;
}