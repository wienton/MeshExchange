#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
// #include <syslog.h> // No longer needed for terminal output
#include <string.h>
#include <errno.h>
#include <time.h>
#include <dirent.h> // For initial directory scan

// Inotify specific headers
#include <sys/inotify.h>

// Buffer for reading inotify events
#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + 16)) // Max 1024 events with names up to 16 chars

// Function to log messages to stdout
void log_message(const char *message) {
    time_t now = time(NULL);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("[%s] %s\n", time_buf, message);
    fflush(stdout); // Ensure output is immediately visible
}

// Function to get file details
void log_file_details(const char *filepath, const char *event_type) {
    struct stat file_stat;
    char log_buf[2048]; // Increased buffer size for full path and details

    // Explicitly add current timestamp for all log entries
    time_t now = time(NULL);
    char current_time_buf[64];
    strftime(current_time_buf, sizeof(current_time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));


    if (stat(filepath, &file_stat) == 0) {
        char mod_time_buf[64];
        strftime(mod_time_buf, sizeof(mod_time_buf), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_mtime));

        snprintf(log_buf, sizeof(log_buf),
                 "[%s] %s | File: %s | Size: %lld bytes | Modified: %s | Perms: %o | UID: %d | GID: %d",
                 current_time_buf, event_type, filepath, (long long)file_stat.st_size, mod_time_buf,
                 (unsigned int)file_stat.st_mode & 0777, // Only file permissions
                 file_stat.st_uid, file_stat.st_gid);
        printf("%s\n", log_buf); // Use printf for terminal output
    } else {
        snprintf(log_buf, sizeof(log_buf), "[%s] %s | File: %s | Error getting details: %s",
                 current_time_buf, event_type, filepath, strerror(errno));
        printf("%s\n", log_buf); // Use printf for terminal output
    }
    fflush(stdout); // Ensure output is immediately visible
}

// Initial scan of the directory
void initial_scan(const char *dir_path) {
    DIR *d;
    struct dirent *dir;
    char log_buf_tmp[2048]; // Temporary buffer for snprintf for the first log

    d = opendir(dir_path);
    if (d) {
        snprintf(log_buf_tmp, sizeof(log_buf_tmp), "Initial scan of directory: %s", dir_path);
        log_message(log_buf_tmp); // Use log_message for consistent timestamp
        while ((dir = readdir(d)) != NULL) {
            // Skip . and ..
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                continue;
            }

            char filepath[PATH_MAX];
            snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, dir->d_name);
            log_file_details(filepath, "INIT_EXISTING");
        }
        closedir(d);
    } else {
        snprintf(log_buf_tmp, sizeof(log_buf_tmp), "Initial scan failed for %s: %s", dir_path, strerror(errno));
        log_message(log_buf_tmp);
    }
}


int main(int argc, char *argv[]) {
    // Check command line arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory_to_watch>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *watch_dir = argv[1];

    // Verify that the directory exists and is a directory
    struct stat st;
    if (stat(watch_dir, &st) == -1) {
        fprintf(stderr, "Error: Directory '%s' does not exist or cannotbe accessed: %s\n",
                watch_dir, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a directory.\n", watch_dir);
        exit(EXIT_FAILURE);
    }

    // --- NO DAEMONIZATION ---
    // The process will run in the foreground and output to the console.

    log_message("Directory watcher started in foreground."); // Use log_message for consistent timestamp

    // 1. Initialize inotify
    int inotify_fd = inotify_init1(IN_NONBLOCK); // Use IN_NONBLOCK for non-blocking read
    if (inotify_fd == -1) {
        log_message("inotify_init1 failed."); // Use log_message
        perror("inotify_init1"); // Still print to stderr for critical error
        exit(EXIT_FAILURE);
    }

    // 2. Add watch for the directory
    int wd = inotify_add_watch(inotify_fd, watch_dir,
                               IN_CREATE | IN_DELETE | IN_MODIFY | IN_ATTRIB | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd == -1) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "inotify_add_watch for %s failed: %s", watch_dir, strerror(errno));
        log_message(err_msg);
        close(inotify_fd);
        exit(EXIT_FAILURE);
    }
    char watch_msg[256];
    snprintf(watch_msg, sizeof(watch_msg), "Watching directory: %s", watch_dir);
    log_message(watch_msg);
    
    // Perform initial scan
    initial_scan(watch_dir);


    // 3. Event loop
    char buffer[EVENT_BUF_LEN];
    while (1) {
        int length = read(inotify_fd, buffer, sizeof(buffer));

        if (length == -1 && errno != EAGAIN) { // EAGAIN means no events available in non-blocking mode, it's normal
            char err_buf[256];
            snprintf(err_buf, sizeof(err_buf), "read from inotify_fd failed: %s", strerror(errno));
            log_message(err_buf);
            break;
        }

        if (length <= 0) { // No events or error other than EAGAIN
            // Sleep for a short period to prevent busy-waiting if no events
            usleep(100000); // Sleep for 100ms
            continue;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &buffer[i];
            char event_name_buf[PATH_MAX];
            char full_path[PATH_MAX];

            // Construct full path for logging
            if (event->len > 0) {
                snprintf(full_path, sizeof(full_path), "%s/%s", watch_dir, event->name);
            } else { // Event might not have a name (e.g., directory itself changed attributes)
                snprintf(full_path, sizeof(full_path), "%s", watch_dir);
            }


            if (event->mask & IN_CREATE) {
                snprintf(event_name_buf, sizeof(event_name_buf), "CREATED %s", (event->mask & IN_ISDIR) ? "[DIR]" : "[FILE]");
                log_file_details(full_path, event_name_buf);
            }
            if (event->mask & IN_DELETE) {
                snprintf(event_name_buf, sizeof(event_name_buf), "DELETED %s", (event->mask & IN_ISDIR) ? "[DIR]" : "[FILE]");
                // For deleted items, stat will fail, log what we have
                time_t now = time(NULL);
                char current_time_buf[64];
                strftime(current_time_buf, sizeof(current_time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
                printf("[%s] %s | File: %s\n", current_time_buf, event_name_buf, full_path); // Log without stat
                fflush(stdout);
            }
            if (event->mask & IN_MODIFY) {
                snprintf(event_name_buf, sizeof(event_name_buf), "MODIFIED %s", (event->mask & IN_ISDIR) ? "[DIR]" : "[FILE]");
                log_file_details(full_path, event_name_buf);
            }
            if (event->mask & IN_ATTRIB) {
                snprintf(event_name_buf, sizeof(event_name_buf), "ATTRIB_CHANGED %s", (event->mask & IN_ISDIR) ? "[DIR]" : "[FILE]");
                log_file_details(full_path, event_name_buf);
            }
            if (event->mask & IN_MOVED_FROM) {
                snprintf(event_name_buf, sizeof(event_name_buf), "MOVED_FROM %s", (event->mask & IN_ISDIR) ? "[DIR]" : "[FILE]");
                log_file_details(full_path, event_name_buf); // Log details before it's gone
            }
            if (event->mask & IN_MOVED_TO) {
                snprintf(event_name_buf, sizeof(event_name_buf), "MOVED_TO %s", (event->mask & IN_ISDIR) ? "[DIR]" : "[FILE]");
                log_file_details(full_path, event_name_buf);
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }

    // Cleanup
    if (wd != -1) {
        inotify_rm_watch(inotify_fd, wd);
    }
    close(inotify_fd);
    log_message("Directory watcher stopped.");

    return EXIT_SUCCESS;
}