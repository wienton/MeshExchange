#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <mongoc/mongoc.h>
#include <bson/bson.h>

#include "config.h"
#include "logs/dblogs.h"

static mongoc_client_t *g_client = NULL;
static mongoc_collection_t *g_collection = NULL;

static int start_docker(void) {
    FILE *fp = fopen(DOCKER_COMPOSE_PATH, "r");
    if (!fp) {
        log_error("docker-compose.yml not found at %s", DOCKER_COMPOSE_PATH);
        return -1;
    }
    fclose(fp);

    if (system("docker-compose up -d") != 0) {
        log_error("Failed to start Docker containers");
        return -1;
    }
    sleep(3); // give MongoDB time to start
    return 0;
}

int mongodb_init(void) {
    mongoc_init();

    for (int attempt = 0; attempt < 3; attempt++) {
        g_client = mongoc_client_new(MONGO_URI);
        if (g_client) {
            bson_t *cmd = BCON_NEW("ping", BCON_INT32(1));
            bson_t reply;
            bson_error_t error;

            bool ok = mongoc_client_command_simple(g_client, "admin", cmd, NULL, &reply, &error);
            bson_destroy(&reply);
            bson_destroy(cmd);

            if (ok) {
                g_collection = mongoc_client_get_collection(g_client, MONGO_DATABASE_NAME, MONGO_COLL_NAME);
                if (g_collection) {
                    log_info("MongoDB connected: %s/%s", MONGO_DATABASE_NAME, MONGO_COLL_NAME);
                    return 0;
                }
            }
            mongoc_client_destroy(g_client);
            g_client = NULL;
        }

        log_warn("MongoDB unreachable, attempt %d/3", attempt + 1);
        if (start_docker() != 0) {
            log_error("Docker start failed — aborting");
            break;
        }
        sleep(2);
    }

    log_error("Failed to initialize MongoDB");
    return -1;
}

int mongodb_insert_file(const char *name, off_t size, mode_t mode, time_t mtime) {
    if (!g_client || !g_collection) {
        log_error("MongoDB not initialized");
        return -1;
    }

    char time_str[32];
    struct tm tm;
    if (localtime_r(&mtime, &tm))
        strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%SZ", &tm);
    else
        strcpy(time_str, "1970-01-01T00:00:00Z");

    bson_t *doc = BCON_NEW(
        "filename", BCON_UTF8(name),
        "size", BCON_INT64((int64_t)size),
        "mode", BCON_INT32((int32_t)(mode & 0777)),
        "mtime", BCON_UTF8(time_str),
        "status", BCON_UTF8("encrypted")
    );

    bson_error_t error;
    bool success = mongoc_collection_insert_one(g_collection, doc, NULL, NULL, &error);

    bson_destroy(doc);
    if (!success) {
        log_error("MongoDB insert failed: %s", error.message);
        return -1;
    }

    log_info("Logged to MongoDB: %s", name);
    return 0;
}

void mongodb_cleanup(void) {
    if (g_collection) {
        mongoc_collection_destroy(g_collection);
        g_collection = NULL;
    }
    if (g_client) {
        mongoc_client_destroy(g_client);
        g_client = NULL;
    }
    mongoc_cleanup();
}