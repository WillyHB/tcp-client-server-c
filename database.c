#include <string.h>  
#include <stdlib.h>
#include <stdio.h>

#define MAX_LINE_SIZE 116


typedef struct Record {
    char handle[32];
    unsigned long int followers;
    char comment[64];
    unsigned long int last_modified;
} Record;

typedef struct Database {
    Record *records;
    int capacity;
    int size;
} Database;

Database db_create();
void db_append(Database *db, Record const *item);
Record *db_index(Database *db, int index);
Record *db_lookup(Database *db, char const *handle);
void db_free(Database *db);
void db_load_csv(Database *db, char const *path);
void db_write_csv(Database *db, char const *path);
Record parse_record(char const *line);


Database db_create() {
    // The database must have initial size 0 and capacity 4
    Database db;
    db.capacity = 4;
    db.size = 0;
    db.records = malloc(sizeof(Record) * 4);
    if (db.records == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }
    return db;
}

void db_append(Database *db, Record const *records) {
    if (db->size >= db->capacity) {
        int new_capacity = db->capacity * 2;
        Record *new_items = malloc(sizeof(Record) * new_capacity);
        if (new_items == NULL) {
            fprintf(stderr, "Failed to allocate memory for database resize.\n");
            exit(1);
        }

        for (int i = 0; i < db->size; i++) {
            new_items[i] = db->records[i];
        }

        free(db->records);
        db->records = new_items;
        db->capacity = new_capacity;
    }

    db->records[db->size] = *records;
    db->size++;
}

Record *db_index(Database *db, int index) {
    // Returns a pointer to the item in the database at the given index.
    // You need to check the bounds of the index.
    if (index < 0 || index >= db->size) {
        return NULL;
    }
    return &db->records[index];
}

Record *db_lookup(Database *db, char const *handle) {
    // Returns a pointer to the first item in the database whose handle
    // field equals the given value
    for (int index = 0; index < db->size; index++) {
        if (strcmp(db->records[index].handle, handle) == 0) {
            return &db->records[index];
        }
    }
    // The handle was not found
    return NULL;
}

void db_free(Database *db) {
    // Releases the memory held by the underlying array.
    // After calling this, the database can no longer be used.
    free(db->records);
    memset(db, 0, sizeof(Database));
}

void db_load_csv(Database *db, char const *path) {
    // Opens file in append mode, creates it if it does not exist
    FILE *f = fopen(path, "a+");
    fclose(f);

    // Open file in read mode
    FILE *file = fopen(path, "r");

    size_t line_size = 0;
    char *buff = NULL;

    // Read each line and parse into a record
    while (getline(&buff, &line_size, file) > 0) {
        Record r = parse_record(buff);
        db_append(db, &r);
    }

    // Free the buffer used by getline
    free(buff);
    fclose(file);
}

void db_write_csv(Database *db, char const *path) {
    // Open file in write mode
    FILE *file = fopen(path, "w+");

    if (file == NULL) {
        fprintf(stderr, "Failed to open csv for writing at path %s\n", path);
        return;
    }

    for (int i = 0; i < db->size; i++) {
        Record *r = db_index(db, i);

        // If index is out of bounds, skip
        if (r == NULL) continue;

        fprintf(file, "%s,%lu,%s,%lu\n",
                r->handle,
                r->followers,
                r->comment,
                r->last_modified);
    }

    fclose(file);
}

Record parse_record(char const *line) {
    Record r;

    // Create a modifiable copy of the input string
    char *dup = strdup(line);

    // Tokenize and populate record fields
    strncpy(r.handle, strtok(dup, ","), 32);
    r.followers = strtoul(strtok(NULL, ","), NULL, 0);
    strncpy(r.comment, strtok(NULL, ","), 64);
    r.last_modified = strtoul(strtok(NULL, ","), NULL, 0);

    free(dup);
    return r;
}

