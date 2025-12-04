#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include <openssl/evp.h>

#define KEY_SIZE        32
#define GCM_TAG_SIZE    16
#define GCM_IV_SIZE     12

// ⚠️ Replace with secure key in production
static const unsigned char g_key[KEY_SIZE] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
};

int is_text_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1) return 0;

    unsigned char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf));
    close(fd);
    if (n <= 0) return 1; // empty = text

    for (ssize_t i = 0; i < n; i++) {
        if (buf[i] == 0) return 0; // binary
    }
    return 1;
}

int encrypt_file(const char *path, mode_t mode, struct timespec mtimes[2])
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

    // Derive IV from filename (first 12 chars, null-padded)
    unsigned char iv[GCM_IV_SIZE] = {0};
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

    // Write back encrypted data + tag
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