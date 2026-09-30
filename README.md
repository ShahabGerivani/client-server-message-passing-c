# Client-Server Message Passing
This project is a simple educational implementation of client-server message passing in C.

**Security warning**: This project is for educational purposes only. The server executes commands on behalf of clients, so any client can read any file the server's user can read. Do not expose it to untrusted users or networks.

The project includes three implementations:
- An I/O multiplexed client/server using Unix domain sockets (`server.c`, `client.c`)
- A multithreaded client/server using Unix domain sockets (`multithread_server.c`, `client.c`)
- A multithreaded client/server using TCP sockets (`multithread_tcp_server.c`, `tcp_client.c`)

The project also includes a program for load testing (`load_test.c`).

# Compiling and Running

## Requirements
- A Unix-like operating system with a C compiler.
- The multithreaded servers use POSIX threads (pthreads).

**Notice**: Always start the server before the client. Each server must be used with its matching client (the TCP server with `tcp_client`, the Unix socket servers with `client`).

## Connection Details
- **Unix domain socket servers** create a socket file at `/tmp/c_recruitment_socket`.
- **TCP server** listens on port `9034`.

## I/O Multiplexed Server (Unix Domain Sockets)

Server:
```
gcc -Wall -o server server.c socket_utils.c command_utils.c queue.c
./server
```

Client (in another terminal):
```
gcc -Wall -o client client.c socket_utils.c
./client
```

## Multithreaded Server (Unix Domain Sockets)

Server:
```
gcc -Wall -pthread -o multithread_server multithread_server.c socket_utils.c command_utils.c
./multithread_server
```

Client (in another terminal):
```
gcc -Wall -o client client.c socket_utils.c
./client
```

## Multithreaded Server (TCP Sockets)

Server:
```
gcc -Wall -pthread -o multithread_tcp_server multithread_tcp_server.c socket_utils.c command_utils.c
./multithread_tcp_server
```

Client (in another terminal):
```
gcc -Wall -o tcp_client tcp_client.c socket_utils.c
./tcp_client
```

## Stopping the Programs
Press `Ctrl+C` in the terminal to stop the server or the client.

# Usage

Once you run both the client and the server, you can give three kinds of input to the client:
- `ls <path>`
- `pwd`
- `cat <absolute filename>`

The client then sends the command to the server, the server runs the command and returns the output back to the client which then shows it to you.

# Load Test
Start a server in one terminal, compile the matching client, then compile and run the load test program in another terminal as follows:
```
gcc -Wall -o load_test load_test.c
./load_test 50 100 ./client
```
This will run the load test with 50 clients and 100 requests for each client using `client` as the binary (you could also use `tcp_client` with the TCP server).
