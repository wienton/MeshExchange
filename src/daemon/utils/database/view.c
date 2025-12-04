#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mongoc/mongoc.h>
#include <bson/bson.h>

#define MONGO_URI           "mongodb://127.0.0.1:27017"
#define DATABASE_NAME       "exchange"
#define COLLECTION_NAME     "file_exchange"
#define POLL_INTERVAL_SEC   5

int main(void)
{
    mongoc_init();

    mongoc_client_t *client = mongoc_client_new(MONGO_URI);
    if (!client) {
        fprintf(stderr, "error: failed to connect to MongoDB\n");
        exit(EXIT_FAILURE);
    }

    mongoc_collection_t *collection =
        mongoc_client_get_collection(client, DATABASE_NAME, COLLECTION_NAME);
    if (!collection) {
        fprintf(stderr, "error: cannot access collection %s.%s\n",
                DATABASE_NAME, COLLECTION_NAME);
        mongoc_client_destroy(client);
        exit(EXIT_FAILURE);
    }

    printf("Viewer active. Checking records every %d seconds...\n", POLL_INTERVAL_SEC);
    printf("Press Ctrl+C to exit.\n");

    while (1) {
        bson_t query = BSON_INITIALIZER;
        mongoc_cursor_t *cursor =
            mongoc_collection_find_with_opts(collection, &query, NULL, NULL);

        printf("\n--- Current Records (%s.%s) ---\n", DATABASE_NAME, COLLECTION_NAME);

        bool found = false;
        const bson_t *doc;
        while (mongoc_cursor_next(cursor, &doc)) {
            char *json = bson_as_relaxed_extended_json(doc, NULL);
            if (json) {
                printf("%s\n", json);
                bson_free(json);
                found = true;
            }
        }

        bson_error_t error;
        if (mongoc_cursor_error(cursor, &error)) {
            fprintf(stderr, "cursor error: %s\n", error.message);
        }

        if (!found) {
            printf("(no records)\n");
        }

        mongoc_cursor_destroy(cursor);
        bson_destroy(&query);

        sleep(POLL_INTERVAL_SEC);
    }

    // Unreachable, but clean in theory
    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
    mongoc_cleanup();
    return 0;
}