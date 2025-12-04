#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "utils/database/config/config.h"
#include "utils/database/logs/dblogs.h"
#include "crypto/crypto.h"
#include "utils/mongo_writter/mongo_wr.h"

#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + NAME_MAX + 1))

static volatile int running = 1;

void signal_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT)
        running = 0;
}

static void handle_file_event(const char *dir, const char *name, int is_delete) {
    char path[PATH_MAX];
    if (snprintf(path, sizeof(path), "%s/%s", dir, name) >= (int)sizeof(path))
        return;

    if (is_delete) {
        log_info("deleted: %s", name);
        return;
    }

    struct stat st;
    if (stat(path, &st) != 0) return;

    struct tm tm;
    char time_str[32] = "unknown";
    if (localtime_r(&st.st_mtime, &tm))
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm);

    log_info("file: %s | size: %ld | mode: %03o | time: %s",
             name, (long)st.st_size, st.st_mode & 0777, time_str);

    if (!is_text_file(path)) {
        log_info("skipped (binary): %s", name);
        return;
    }

    struct timespec mtimes[2] = { st.st_mtim, st.st_mtim };
    if (!encrypt_file(path, st.st_mode, mtimes)) {
        log_error("encrypt failed: %s", name);
        return;
    }

    log_info("encrypted: %s", name);
    if (mongodb_insert_file(name, st.st_size, st.st_mode, st.st_mtime) != 0) {
        log_error("failed to log to MongoDB: %s", name);
    }
}

int main(void) {
    // Daemonize
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) _exit(0);

    if (setsid() < 0) exit(EXIT_FAILURE);
    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) _exit(0);

    umask(0);
    if (chdir("/") != 0) exit(EXIT_FAILURE);

    close(STDIN_FILENO); close(STDOUT_FILENO); close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);

    // Initialize
    if (dblog_init(LOG_PATH) != 0) {
        exit(EXIT_FAILURE);
    }

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    if (mongodb_init() != 0) {
        dblog_close();
        exit(EXIT_FAILURE);
    }

    log_info("Daemon started, watching: %s", WATCH_DIR);

    int in_fd = inotify_init1(IN_NONBLOCK);
    if (in_fd == -1) {
        log_error("inotify_init1 failed");
        mongodb_cleanup();
        dblog_close();
        exit(EXIT_FAILURE);
    }

    int wd = inotify_add_watch(in_fd, WATCH_DIR, IN_CREATE | IN_CLOSE_WRITE | IN_DELETE);
    if (wd == -1) {
        log_error("inotify_add_watch failed");
        close(in_fd);
        mongodb_cleanup();
        dblog_close();
        exit(EXIT_FAILURE);
    }

    char buf[EVENT_BUF_LEN];
    while (running) {
        ssize_t len = read(in_fd, buf, sizeof(buf));
        if (len == -1) {
            if (errno == EAGAIN) { usleep(100000); continue; }
            break;
        }

        for (char *ptr = buf; ptr < buf + len; ) {
            struct inotify_event *e = (struct inotify_event *)ptr;
            if (e->len && e->name[0] != '.') {
                if (e->mask & (IN_CREATE | IN_CLOSE_WRITE)) {
                    handle_file_event(WATCH_DIR, e->name, 0);
                } else if (e->mask & IN_DELETE) {
                    handle_file_event(WATCH_DIR, e->name, 1);
                }
            }
            ptr += sizeof(struct inotify_event) + e->len;
        }
    }

    // Cleanup
    inotify_rm_watch(in_fd, wd);
    close(in_fd);
    mongodb_cleanup();
    dblog_close();
    log_info("Daemon stopped");
    return 0;
}