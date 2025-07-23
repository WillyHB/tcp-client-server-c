#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "database.c"
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8080 
#define BACKLOG 10
#define LS_SIZE 80

enum command {
    ADD,
    LIST,
    UPDATE,
    SORT,
    FIND,
    SWAP,
    SAVE,
    EXIT,
};

//Shouldn't be necessary, compiled together...
//#pragma pack(push, 1)
typedef struct client_data {
    enum command command;
    union args {
        struct add {
            char handle[32];
            char followers[10];
            char comment[64];
        } add;
        struct update {
            char handle[32];
            char new_followers[10];
            char comment[64];
        } update;
        struct find {
            char handle[32];
        } find;
        struct swap {
            char handle1[32];
            char handle2[32];
        } swap;
    } args;

} client_data;

typedef enum return_type {
    ERROR,
    MESSAGE,
    SIGNAL,
} return_type;

typedef struct server_response {
    
    return_type return_type;
    union data {

        struct error {
            int err_code;
            char err_message[256];
        } error;

        struct message {
            int return_message_length;
            int data_chunk_size;
        } message;

        struct signal {
            int code;
        } signal;
    } data;
} server_response;

//#pragma pop

int     parse_data(Database*, client_data*, int);

void 	list(int fd, Database*, client_data*);
void 	find(int fd, Database*, client_data*);
void    sort(int fd, Database*, client_data*);
int  	add(int fd, Database*, client_data*);
int  	update(int fd, Database*, client_data*);
int  	swap(int fd, Database*, client_data*);

int 	main_loop(int);
int     prog(Database*);

void return_error(int fd, int code, char* message);
void return_message(int fd, int message_len, int chunk_size);
void return_signal(int fd, int code);

// hacky
int unsaved_changes;

int main(int argc, char *argv[])
{
    Database db = db_create();
    db_load_csv(&db, "database.csv");
    printf("Loaded %d records.\n", db.size);

    int code = prog(&db);
    db_free(&db);
    return code;
}

int prog(Database* db) {

    struct sockaddr_in server_addr;
        
    socklen_t socklen = sizeof(struct sockaddr);
    int opt = 1; 
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0 );

    if (server_fd < 0) {
        perror("socket failed");
        return -1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsocketopt failed");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr)) <= 0) {
        fprintf(stderr, "Error setting address of server\n");
    }

    if (bind(server_fd, (struct sockaddr*)&server_addr, socklen) < 0) {
        perror("bind failed");
        return -1;
    }

    // 3 is the amount of pending connections queue will hold
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listening failed");
        return -1;
    }

    printf("main server: waiting for connections...\n");

    // Master list of all filedescriptors
    fd_set master;
    
    // set used to check which filedescriptors have changes
    fd_set readset;
    int fdmax = server_fd;

    // zero-initialise fd sets
    FD_ZERO(&master);
    FD_ZERO(&readset);

    // add server_fd (listener) to master set
    FD_SET(server_fd, &master);

    // struct for client data
    client_data data;

    while (1) {

        readset = master;

        if (select(fdmax+1, &readset, NULL, NULL, NULL) == -1) {
            perror("select failed");
            return -1;
        }

        for (int i = 0; i <= fdmax; i++) {

            // ready to read
            if (FD_ISSET(i, &readset)) {

                // new connection
                if (i == server_fd) {
                    int new_socket = accept(server_fd, (struct sockaddr*)&server_addr, &socklen);

                    if (new_socket < 0) {
                        perror("accept failed");
                        close(new_socket);
                        return -1;
                    }

                    FD_SET(new_socket, &master);

                    if (new_socket > fdmax) { fdmax = new_socket; }

                    printf("client connected!\n");
                } else {
                    
                    if (read(i, &data, sizeof(data)) <= 0) {
                        fprintf(stderr, "client closed or error\n");
                        printf("closing client...\n");
                        close(i);
                        FD_CLR(i, &master);
                    } else {
                        int temp = 0;
                        temp += parse_data(db, &data, i);
                        unsaved_changes += temp >= 0 ? temp : -unsaved_changes;
                    }
                }
            }
        }
    }
}

int parse_data(Database *db, client_data *data, int fd) {

    switch (data->command) {
        case ADD:
            return add(fd, db, data);
            // sends like just error code or nothing
            // makes change

        case LIST:
            list(fd, db, data);
            return 0;
            // returns list of all records

        case UPDATE:
            return update(fd, db, data);
            // makes change
            
        case SORT:
            sort(fd, db, data);
            return -1;
            // returns success message
            // makes change but saves

        break;

        case FIND:
            find(fd, db, data);
            return 0;
            // returns list of all records or error or nothing

        break;

        case SWAP:
            return swap(fd, db, data);
            // returns error or nothing
            // makes change

        return 1;

        case EXIT:
        // so == 0, there are no changes
            if (!unsaved_changes) {
                return_signal(fd, 1);
            } else {
                return_error(fd, 6, "Unsaved changes on the server. Use 'exit fr' to force exiting anyway (Changes will stay unless server is closed)\n");
            }

        break;

        case SAVE:
            // Write records to the CSV file:
            db_write_csv(db, "database.csv");
            return_message(fd, 32, 32);

            char buff[32];
            sprintf(buff, "Wrote %d records.\n", db->size);

            send(fd, buff, 32, 0);
        return -1;
    }

    return 0;
}

void list(int fd, Database *db, client_data *data) {

    return_message(fd, LS_SIZE*2 + db->size * LS_SIZE, LS_SIZE);

    char buff[LS_SIZE] = { 0 };
    sprintf(buff, "| HANDLE               | FOLLOWERS  | LAST MODIFIED    | COMMENT              |");
    buff[LS_SIZE-1] = '\n';
    send(fd, buff, LS_SIZE, 0);
    sprintf(buff, "|----------------------|------------|------------------|----------------------|");
    buff[LS_SIZE-1] = '\n';
    send(fd,buff,LS_SIZE,0);

    for (int i = 0; i < db->size; i++) {
        Record *rec = &db->records[i];
        char time_str[20];
        time_t ts = rec->last_modified;
        struct tm *tm_info = localtime(&ts);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);

        // Print a row for each record (truncating if necessary): 
        sprintf(buff, "| %-20.20s | %-10lu | %-16.16s | %-20.20s |", 
        rec->handle, rec->followers, time_str, rec->comment); //PRINTS: 5 + 22 + 12 + 18 + 22 = 79 characters + newline NO null terminator = 80 :)
        buff[LS_SIZE-1] = '\n';
        send(fd,buff,LS_SIZE,0);
    }
}

int add(int fd, Database *db, client_data *data) {
    /*
  	// Check handle length vs. our Record's array size:
	if (strlen(data->args.add.handle) >= sizeof(((Record *)0)->handle)) {
    	printf("Error: handle too long.\n");
        return 0;
	}*/

    // Ensure the handle does not already exist in the database:
    if (db_lookup(db, data->args.add.handle) != NULL) {

        char buff[64] = { 0 };
    	sprintf(buff, "handle %s already exists.\n", data->args.add.handle);
        return_error(fd, 1, buff);
        return 0;
    }

    // Convert the follower count to an unsigned long:
    char *endptr;
    unsigned long followers = strtoul(data->args.add.followers, &endptr, 10);

    if (*endptr != '\0') {
        return_error(fd, 2, "follower count must be an integer\n");
        return 0;
    }

    // Verify no commas in the comment:
    if (strchr(data->args.add.comment, ',') != NULL) {
        return_error(fd, 3, "handle cannot contain commas.\n");
        return 0;
    }

    // Construct and append the new record:
    Record new_rec;
    strncpy(new_rec.handle, data->args.add.handle, 32 - 1);
    new_rec.handle[sizeof(new_rec.handle) - 1] = '\0';
    new_rec.followers = followers;
    strncpy(new_rec.comment, data->args.add.comment, 64 - 1);
    new_rec.comment[sizeof(new_rec.comment) - 1] = '\0';
    new_rec.last_modified = (unsigned long)time(NULL);

    db_append(db, &new_rec);

    return_signal(fd, 0);
	return 1;
}

int update(int fd, Database *db, client_data *data) {
    
    // Locate the record by handle:
    Record *rec = db_lookup(db, data->args.update.handle);
    if (rec == NULL) {
        char buff[64] = { 0 };
        sprintf(buff, "no entry with handle %s\n", data->args.update.handle);
        return_error(fd, 4, buff);
        return 0;
    }

    // Parse the new follower count:
    char *endptr;
    unsigned long followers = strtoul(data->args.update.new_followers, &endptr, 10);
    if (*endptr != '\0') {
        return_error(fd, 2, "follower count must be an integer\n");
        return 0;
    }

    if (strchr(data->args.update.comment, ',') != NULL) {
        return_error(fd, 3, "comment cannot contain commas.\n");
        return 0;
    }

    // Update the record:
    rec->followers = followers;
    strncpy(rec->comment, data->args.update.comment, sizeof(rec->comment) - 1);
    rec->comment[sizeof(rec->comment) - 1] = '\0';
    rec->last_modified = (unsigned long)time(NULL);

    return_signal(fd, 0);
    return 1;
}


int compare_records(const void *a, const void *b) {
    const Record *ra = (const Record *)a;
    const Record *rb = (const Record *)b;
    return strcmp(ra->handle, rb->handle);
}

void sort(int fd, Database *db, client_data *data) {
    // Sort records by handle, then save immediately:
    qsort(db->records, db->size, sizeof(Record), compare_records);
    db_write_csv(db, "database.csv");
    
    char *message = "Database sorted and written to file\n";

    return_message(fd, 64, 64);

    send(fd, message, 64, 0);
}

void find(int fd, Database *db, client_data *data) {

    Record *rec = db_lookup(db, data->args.find.handle);
    if (rec == NULL) {
        char buff[64];
        sprintf(buff, "handle %s not found in database\n", data->args.find.handle);
        return_error(fd, 5, buff);
        return;
    }

	//Prints out the table
	list(fd, db, data);
}

int swap(int fd, Database *db, client_data *data) {

    // Look up both records
    Record *rec1 = db_lookup(db, data->args.swap.handle1);
    Record *rec2 = db_lookup(db, data->args.swap.handle2);

    // Check if either record is missing
    if (rec1 == NULL || rec2 == NULL) {
        return_error(fd, 5, "handles not found in database\n");
        return 0;
    }

    // Calculate their indices in the array
    int index1 = rec1 - db->records;
    int index2 = rec2 - db->records;

    // Swap the two records
    Record temp = db->records[index1];
    db->records[index1] = db->records[index2];
    db->records[index2] = temp;

    return_signal(fd, 0);
    return 1;
}

void return_error(int fd, int code, char* message) {
    server_response response;
    response.return_type = ERROR;
    response.data.error.err_code = code;
    strncpy(response.data.error.err_message, message, 256);
    send(fd, &response, sizeof(response), 0);
}

void return_message(int fd, int length, int chunk) {
    server_response response;
    response.return_type = MESSAGE;
    response.data.message.return_message_length = length;
    response.data.message.data_chunk_size = chunk;
    send(fd, &response, sizeof(response), 0);
}

void return_signal(int fd, int code) {
    server_response response;
    response.return_type = SIGNAL;
    response.data.signal.code = code;
    send(fd, &response, sizeof(response), 0);
}
