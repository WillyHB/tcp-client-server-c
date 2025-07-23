#include <string.h>  
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080

int connect_to_server();

typedef enum command {
    ADD,
    LIST,
    UPDATE,
    SORT,
    FIND,
    SWAP,
    SAVE,
    EXIT,
} command;

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

        // If success, we wait to get return_message_length number of chars from server before we continue
        struct message {
            int return_message_length;
            int data_chunk_size;
        } message;

        // for sending short signals, for program only
        struct signal {
            int code;
        } signal;
    } data;
} server_response;


void 	list();
void 	find();
void 	sort();

int 	add();
int 	update();
int 	swap();
int 	main_loop(int);
int     await_response(int);

int main() {

    int client_fd = connect_to_server();

    if (client_fd < 0) {
        printf("Error connecting to server");
        return -1;
    }

    main_loop(client_fd);

    printf("closing client\n");
    close(client_fd);
    return 0;
}

int connect_to_server() {
    struct sockaddr_in server_addr;

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (client_fd < 0) {
        perror("socket creation failed");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr)) <= 0) {
        fprintf(stderr, "error setting address of server\n");
        return -1;
    }
    
    int status = connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    if (status < 0) {
        perror("failed to connect\n");
        return -1;
    }

    printf("client: connected to server!\n");
    return client_fd;
}


int main_loop(int fd) {

    int awaiting_response = 0;
    char input[256];

    while (1) {
        printf("> ");

        // Read a line of input:
        if (!fgets(input, sizeof(input), stdin))
            break;

        // Remove trailing newline if present:
        input[strcspn(input, "\n")] = 0;

        // Skip if the user just pressed enter (empty line):
        if (strlen(input) == 0) {
            continue;
		}

        // Tokenize the command input:
        char *command = strtok(input, " ");
        if (command == NULL) { continue; }

        client_data data;

        if (strcmp(command, "list") == 0) {
            data.command = LIST;

        } else if (strcmp(command, "sort") == 0) {
            data.command = SORT;

        } else if (strcmp(command, "add") == 0) {
            // Expect two arguments: HANDLE and FOLLOWERS.
            char *handle = strtok(NULL, " ");
            char *followers = strtok(NULL, " ");

            if (handle == NULL || followers == NULL) {
                printf("Error: usage: update HANDLE FOLLOWERS\n");
                continue;
            }

            // Prompt for the comment:
            printf("Comment> ");
            char comment[64];
            if (!fgets(comment, sizeof(comment), stdin)) { return 0; }
            comment[strcspn(comment, "\n")] = 0;

            data.command = ADD;
            strncpy(data.args.add.handle, strdup(handle), 32);
            strncpy(data.args.add.followers, strdup(followers), 10);
            strncpy(data.args.add.comment, comment, 64);

        } else if (strcmp(command, "update") == 0) {
            // Expect two arguments: HANDLE and FOLLOWERS.
            char *handle = strtok(NULL, " ");
            char *followers = strtok(NULL, " ");

            if (handle == NULL || followers == NULL) {
                printf("Error: usage: update HANDLE FOLLOWERS\n");
                continue;
            }

            // Prompt for a new comment:
            printf("Comment> ");
            char comment[64];
            if (!fgets(comment, sizeof(comment), stdin)) { return 0; }
            comment[strcspn(comment, "\n")] = 0;

            data.command = UPDATE;
            strncpy(data.args.update.handle, strdup(handle), 32);
            strncpy(data.args.update.new_followers,  strdup(followers), 10);
            strncpy(data.args.update.comment, comment, 64);

        }  else if (strcmp(command, "find") == 0) {
            char *handle = strtok(NULL, " ");

            if (handle == NULL) {
                printf("Error: usage: find HANDLE\n");
                continue;
            }

            data.command = FIND;
            strncpy(data.args.find.handle, strdup(handle), 32);

        } else if (strcmp(command, "swap") == 0) {
            // Get the two handles from user input
            char *handle1 = strtok(NULL, " ");
            char *handle2 = strtok(NULL, " ");
	        		  
	        if (handle1 == NULL || handle2 == NULL) {
	        	printf("Error: usage: swap HANDLE1 HANDLE2\n");
                continue;
	        }

            data.command = SWAP;
            strncpy(data.args.swap.handle1, strdup(handle1), 32);
            strncpy(data.args.swap.handle2, strdup(handle2), 32);

        } else if (strcmp(command, "save") == 0) {
            data.command = SAVE;

        } else if (strcmp(command, "exit") == 0) {
            // Check for optional "fr" parameter to force exit:
            char *opt = strtok(NULL, " ");
            if (opt != NULL) {
                if (strcmp(opt, "fr") == 0) {
                    // just close, it's forced
                    break;
                }
            }

            data.command = EXIT;
        } else {
            // Unknown command:
            printf("Error: unknown command.\n");
            continue;
        }

        send(fd, &data, sizeof(data), 0);
        if (await_response(fd) < 0) { break; }
    }

    return 0;
}

int await_response(int fd) {

    server_response response;
    int d = read(fd, &response, sizeof(response));

    if (!d) {
        printf("server error\n");
        return -1;
    }

    switch (response.return_type) {
        case MESSAGE:

            int bytes_read = 0;
            int chunk_size = response.data.message.data_chunk_size;
            char *success_buff = malloc(sizeof(char)*chunk_size);

            while (bytes_read < response.data.message.return_message_length) {

                bytes_read += read(fd, success_buff, chunk_size);
                int b = printf("%s", success_buff);
            }

            free(success_buff);
            break;

        case ERROR:
            fprintf(stderr, "Error %d: %s", response.data.error.err_code, response.data.error.err_message);
            // Special kill-code, not necessarily an error
            if (response.data.error.err_code == -1) {
                return -1;
            }
            break;

        case SIGNAL:

            switch (response.data.signal.code) {
                // Success
                case 0:
                    return 0;

                // Kill signal
                case 1:
                    return -1;
            }
    }

    return 0;
}

