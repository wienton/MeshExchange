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

// Fixed key — replace in real use!
static const unsigned char g_key[KEY_SIZE] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
};

static int is_text_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1) return 0;

    unsigned char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf));
    close(fd);
    if (n <= 0) return 1; // empty → treat as text

    for (ssize_t i = 0; i < n; i++) {
        if (buf[i] == 0) return 0; // binary
    }
    return 1;
}

static int encrypt_in_place(const char *path, mode_t orig_mode, struct timespec mtimes[2])
{
    int fd = open(path, O_RDONLY);
    if (fd == -1) return 0;

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return 0;
    }
    off_t len = st.st_size;
    if (len > SSIZE_MAX) {
        close(fd);
        return 0;
    }

    unsigned char *plaintext = malloc(len);
    if (!plaintext) {
        close(fd);
        return 0;
    }

    ssize_t n = read(fd, plaintext, len);
    close(fd);
    if (n != len) {
        free(plaintext);
        return 0;
    }

    // Derive deterministic IV from path (for demo — not ideal for all cases)
    unsigned char iv[GCM_IV_SIZE] = {0};
    for (size_t i = 0; path[i] && i < GCM_IV_SIZE; i++)
        iv[i] = path[i];

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        free(plaintext);
        return 0;
    }

    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, g_key, iv)) {
        EVP_CIPHER_CTX_free(ctx);
        free(plaintext);
        return 0;
    }

    int outlen;
    unsigned char *ciphertext = malloc(len + GCM_TAG_SIZE);
    if (!ciphertext) {
        EVP_CIPHER_CTX_free(ctx);
        free(plaintext);
        return 0;
    }

    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &outlen, plaintext, len)) goto fail;
    int clen = outlen;

    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + clen, &outlen)) goto fail;
    clen += outlen;

    unsigned char tag[GCM_TAG_SIZE];
    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE, tag)) goto fail;

    // Write encrypted data + tag back to same file
    fd = open(path, O_WRONLY | O_TRUNC);
    if (fd == -1) goto fail;

    if (write(fd, ciphertext, clen) != clen) {
        close(fd);
        goto fail;
    }
    if (write(fd, tag, GCM_TAG_SIZE) != GCM_TAG_SIZE) {
        close(fd);
        goto fail;
    }
    close(fd);

    // Restore original permissions
    chmod(path, orig_mode);
    // Optional: restore mtime (commented for simplicity)
    // futimens(fd, mtimes);

    EVP_CIPHER_CTX_free(ctx);
    free(plaintext);
    free(ciphertext);
    return 1;

fail:
    EVP_CIPHER_CTX_free(ctx);
    free(plaintext);
    free(ciphertext);
    return 0;
}

static void print_and_maybe_encrypt(const char *dir, const char *name)
{
    char path[PATH_MAX];
    if (snprintf(path, sizeof(path), "%s/%s", dir, name) >= (int)sizeof(path))
        return;

    struct stat st;
    if (stat(path, &st) != 0) return;

    char time_buf[32];
    struct tm tm;
    if (localtime_r(&st.st_mtime, &tm) &&
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm)) {
        /* ok */
    } else {
        strcpy(time_buf, "unknown");
    }

    printf("[FILE] name: %s | size: %ld bytes | perm: %03o | mtime: %s\n",
           name, (long)st.st_size, st.st_mode & 0777, time_buf);

    if (!is_text_file(path))
        return;

    if (encrypt_in_place(path, st.st_mode, NULL)) {
        printf("[ENCRYPTED] %s\n", name);
    } else {
        printf("[ERROR] encryption failed: %s\n", name);
    }
}

static void handle_event(const char *dir, const char *name, int is_delete)
{
    if (is_delete) {
        printf("[DELETE] %s\n", name);
        return;
    }
    print_and_maybe_encrypt(dir, name);
}

int main(void)
{
    const char *dir = "../../view";
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a valid directory\n", dir);
        exit(EXIT_FAILURE);
    }

    int in_fd = inotify_init1(IN_NONBLOCK);
    if (in_fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }

    int wd = inotify_add_watch(in_fd, dir,
                               IN_CREATE | IN_DELETE | IN_MODIFY);
    if (wd == -1) {
        perror("inotify_add_watch");
        close(in_fd);
        exit(EXIT_FAILURE);
    }

    printf("Monitoring %s (text files encrypted in-place)\n", dir);
    char buf[EVENT_BUF_LEN];

    for (;;) {
        ssize_t len = read(in_fd, buf, sizeof(buf));
        if (len == -1) {
            if (errno == EAGAIN) {
                usleep(100000);
                continue;
            } else {
                perror("read");
                break;
            }
        }

        for (ssize_t i = 0; i < len; ) {
            struct inotify_event *e = (struct inotify_event *)(buf + i);
            if (e->len) {
                if (e->mask & (IN_CREATE | IN_MODIFY)) {
                    handle_event(dir, e->name, 0);
                }
                if (e->mask & IN_DELETE) {
                    handle_event(dir, e->name, 1);
                }
            }
            i += sizeof(struct inotify_event) + e->len;
        }
        fflush(stdout);
    }

    inotify_rm_watch(in_fd, wd);
    close(in_fd);
    return 0;
}