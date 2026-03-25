# Secure Multi-Threaded C FTP Server & Client

A custom File Transfer Protocol (FTP) application built in C. This project demonstrates network programming, concurrent thread management and cryptographic security practices. 

Built and tested on Windows Subsystem for Linux (WSL) environment.

## Key Features

* **Multi-Threaded Architecture:** Utilizes POSIX threads to handle multiple concurrent client connections without blocking.
* **Client-Side SHA-256 Hashing:** Password hashing for secure transmission over the network using OpenSSL 3.0.
* **Chunk Transfers:** Buffered disk reads/writes into safe 4KB network chunks, ensuring reliable file streaming.
* **Built-in Security:** Features precausions against Path Traversal attacks and includes an user Blacklist system.

## Prerequisites

To compile and run this project, you need `gcc` and the OpenSSL development headers installed on your Linux/WSL machine.

```bash
sudo apt update
sudo apt install build-essential libssl-dev -y
```
## Compilation

Clone the repository and compile the client and server using the following commands. Note that the cryptography (-lcrypto) and threading (-pthread) libraries must be linked.

# Server:
```bash
gcc serverP.c -o server -pthread -lcrypto
```

# Client:
```bash
gcc clientP.c -o client -lcrypto
```

## Usage

### 1. Start the Server
Run the server executable. It will automatically bind to port 4000 and listen for incoming connections.
```bash
./server
```

### 2. Connect the Client
In a new terminal instance, run the client executable, passing the server's IP address and the port number (4000) as arguments.
```bash
./client 127.0.0.1 4000
```

### 3. Authentication
The first time you enter a username, the server will ask you to create a password.
On subsequent logins, it will verify your hashed password against the login database.
 Note: The login and blacklist databases are generated automatically in the server's directory.

## Available Commands
Once authenticated, the client will present a menu with the following operations:

`1 - get`: Download a file from the server to your local machine.

`2 - put`: Upload a local file from your machine to the server.

`3 - blacklist`: Add a specific username to the server's blacklist, permanently blocking their access.

`4 - quit`: Safely close the socket connection and exit the program.

## Author
### Bibire Constantin-Mihnea
- [LinkedIn](https://www.linkedin.com/in/constantin-mihnea-bibire-905277305/)
