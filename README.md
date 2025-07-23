## tcp-client-server-c
A basic networked client-server database written in C for an intro to software systems programming course. It simulates a minimal social media backend, supporting user handles, follower counts, and comments. Originally built as an 'extra' course for an introductory systems programming course, this project utilises raw TCP networking, I/O multiplexing with `select()`/`poll()`, and low-level binary protocols for structured data exchange — all without relying on third-party libraries or databases.

The system allows multiple clients to interact with a shared database of "social accounts," each with a handle, follower count, and comment list. Clients can create, modify, and view account information. All data is persisted in a CSV file, giving the server durability across sessions.

The code follows strict style guidlines which I won't post for copyright reasons.

## Purpose
As stated previously, this was written for a class, following a specific style guideline and structure criteria.  

## Key Features

- Built in C using POSIX TCP sockets
- Supports multiple concurrent clients with `select()` and `poll()`
- Clients can:
  - Add and update user accounts
  - View and sort data
  - Swap accounts
  - Save to and load from persistent CSV storage
  - Sort records
- Clean internal protocol using `structs and tagged enums


## Technical details

I have 2 structs which I use to send data back and forth: `client_data`, which sends data from the client to the server, and `server_response`, which sends data from the server to the client.

The client_data struct contains 2 fields:
1. command, which is an enum containing the type of the command the client is sending.
2. args, which is a union containing struct fields which correspond to the data the client will send to the server during a certain command.
(Example: struct add contains `char[32] handle;` `char[10] followers;` `char[64] comment;`)
There's no structs for commands such as find and exit—they send no data.

When the user inputs a command and it's parsed by the client, the corresponding fields are filled into a client_data. It is then sent using the `send()` function to the socket file descriptor.

The server continuously loops, using the `select()` function to check for changes in socket file descriptors. (If there's a change in the server_fd, it means a new client wants to connect.) If one of the client file descriptors is ready to be read, the server reads the data as a client_data struct, and passes this to the `parse_data()` function to be parsed and processed.

Once the server has parsed and processed, it sends back the data through the server_response struct.

server_response contains 2 fields:
1. return_type, which is an enum containing the possible return types, of which there are three: ERROR, MESSAGE, SIGNAL.
2. data, which is a union containing a struct for each of the return_type enum members.

Struct error contains an int for the error code, and a char array to hold an error message.
Struct message contains the length of the message which will be returned, and the size of the chunks if it is split up over multiple `send()` calls.
Struct signal contains a signal code, which the client can process.

It then uses the `send()` function to send to the client file descriptor, and continues the loop, checking all other connected clients.

After having sent the command to the server, the client awaits a response. It waits with the `read()` function until it receives a server_response struct. Once it does, it checks the type:
If it's a MESSAGE type, it awaits a further stream of bytes. It expects as many as were stated in the server_response.data.message.length field, so it reads from the socket file descriptor until it receives all bytes, printing the message as it receives it to stdout. It reads server_response.data.message.chunk_size number of bytes at a time.
If it's an ERROR type, it outputs the error code and message to the client stdout, and then checks if the error code is -1. If it is, the client shuts down; otherwise it continues.
If it's a SIGNAL type, it processes the signal. In my program there's only 2 implemented signals:
*0 — indicates the server successfully processed the command with no message
*1 — kills the program but without the error
