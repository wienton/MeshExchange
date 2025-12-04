#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include <openssl/evp.h>

#define EVENT_BUF_LEN (1024 * (sizeof(struct inotify_event) + NAME_MAX + 1))
#define KEY_SIZE 32
#define GCM_TAG_SIZE 16
#define GCM_IV_SIZE 12

static const unsigned char g_key[KEY_SIZE] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
};

static int is_text_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1) return 0;

    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return 0; }

    // Skip empty or very small files (could be encrypted)
    if (st.st_size < 16) {
        close(fd);
        return 1;
    }

    unsigned char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf));
    close(fd);
    if (n <= 0) return 1;

    for (ssize_t i = 0; i < n; i++) {
        if (buf[i] == 0) return 0;
    }
    return 1;
}

static int encrypt_file(const char *path, mode_t mode, struct timespec mtimes[2])
{
    int fd = open(path, O_RDONLY);
    if (fd == -1) return 0;

    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return 0; }
    off_t len = st.st_size;
    if (len == 0) { close(fd); return 1; }

    unsigned char *plaintext = malloc(len);
    if (!plaintext) { close(fd); return 0; }

    ssize_t n = read(fd, plaintext, len);
    close(fd);
    if (n != len) { free(plaintext); return 0; }

    // IV: first 12 bytes of filename (null-padded)
    unsigned char iv[GCM_IV_SIZE] = {0};
    size_t name_len = strlen(path);
    const char *base = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    strncpy((char*)iv, base, GCM_IV_SIZE - 1);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { free(plaintext); return 0; }

    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, g_key, iv)) goto fail;

    unsigned char *ciphertext = malloc(len + GCM_TAG_SIZE);
    if (!ciphertext) goto fail;

    int outlen, clen = 0;
    if (!EVP_EncryptUpdate(ctx, ciphertext, &outlen, plaintext, len)) {
        free(ciphertext); goto fail;
    }
    clen = outlen;

    if (!EVP_EncryptFinal_ex(ctx, ciphertext + clen, &outlen)) {
        free(ciphertext); goto fail;
    }
    clen += outlen;

    unsigned char tag[GCM_TAG_SIZE];
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE, tag)) {
        free(ciphertext); goto fail;
    }

    // Write back
    fd = open(path, O_WRONLY | O_TRUNC);
    if (fd == -1) { free(ciphertext); goto fail; }

    if (write(fd, ciphertext, clen) != clen ||
        write(fd, tag, GCM_TAG_SIZE) != GCM_TAG_SIZE) {
        close(fd); free(ciphertext); goto fail;
    }

    fchmod(fd, mode);
    futimens(fd, mtimes);
    close(fd);

    EVP_CIPHER_CTX_free(ctx);
    free(plaintext);
    free(ciphertext);
    return 1;

fail:
    EVP_CIPHER_CTX_free(ctx);
    free(plaintext);
    return 0;
}

static void handle_file(const char *dir, const char *name, int is_delete)
{
    char path[PATH_MAX];
    if (snprintf(path, sizeof(path), "%s/%s", dir, name) >= (int)sizeof(path))
        return;

    if (is_delete) {
        printf("deleted: %s\n", name);
        return;
    }

    struct stat st;
    if (stat(path, &st) != 0) return;

    struct tm tm;
    char time_str[32] = "unknown";
    if (localtime_r(&st.st_mtime, &tm))
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm);

    printf("file: %s | size: %ld | mode: %03o | time: %s\n",
           name, (long)st.st_size, st.st_mode & 0777, time_str);

    if (!is_text_file(path)) {
        printf("skipped (binary): %s\n", name);
        return;
    }

    struct timespec mtimes[2] = { st.st_mtim, st.st_mtim };
    if (encrypt_file(path, st.st_mode, mtimes)) {
        printf("encrypted: %s\n", name);
    } else {
        printf("encrypt failed: %s\n", name);
    }
}

int main(void)
{
    const char *dir = "../../view";
    if (access(dir, F_OK) != 0 || access(dir, R_OK|W_OK) != 0) {
        fprintf(stderr, "error: directory not accessible: %s\n", dir);
        exit(EXIT_FAILURE);
    }

    int in_fd = inotify_init1(IN_NONBLOCK);
    if (in_fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }

    int wd = inotify_add_watch(in_fd, dir, IN_CREATE | IN_DELETE | IN_CLOSE_WRITE);
    if (wd == -1) {
        perror("inotify_add_watch");
        close(in_fd);
        exit(EXIT_FAILURE);
    }

    printf("watching: %s\n", dir);
    char buf[EVENT_BUF_LEN];

    for (;;) {
        ssize_t len = read(in_fd, buf, sizeof(buf));
        if (len == -1) {
            if (errno == EAGAIN) { usleep(100000); continue; }
            break;
        }

        for (char *ptr = buf; ptr < buf + len; ) {
            struct inotify_event *e = (struct inotify_event *)ptr;
            if (e->len && e->name[0] != '.') {
                if (e->mask & (IN_CREATE | IN_CLOSE_WRITE)) {
                    handle_file(dir, e->name, 0);
                } else if (e->mask & IN_DELETE) {
                    handle_file(dir, e->name, 1);
                }
            }
            ptr += sizeof(struct inotify_event) + e->len;
        }
        fflush(stdout);
    }

    inotify_rm_watch(in_fd, wd);
    close(in_fd);
    return 0;
}