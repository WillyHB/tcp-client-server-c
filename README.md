## tcp-client-server-c
A basic networked client-server database written in C for an intro to software systems programming course. It simulates a minimal social media backend, supporting user handles, follower counts, and comments. Originally built as an 'extra' course for an introductory systems programming course, this project utilises raw TCP networking, I/O multiplexing with `select()`/`poll()`, and low-level binary protocols for structured data exchange — all without relying on third-party libraries or databases.

The system allows multiple clients to interact with a shared database of "social accounts," each with a handle, follower count, and comment list. Clients can create, modify, and view account information. All data is persisted in a CSV file, giving the server durability across sessions.

The code follows strict style guidlines which I won't post for copyright reasons.

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

## Purpose
As stated previously, this was written for a class, following a specific style guideline and structure criteria.  
