# Compiling and Running
## Server
```
gcc -Wall -o server server.c socket_utils.c command_utils.c queue.c 
sudo ./server
```
or
```
gcc -Wall -o multithread_server multithread_server.c socket_utils.c command_utils.c
sudo ./multithread_server
```
or
```
gcc -Wall -o multithread_tcp_server multithread_tcp_server.c socket_utils.c command_utils.c
sudo ./multithread_tcp_server
```
## Client
```
gcc -Wall -o client client.c socket_utils.c
sudo ./client
```
or
```
gcc -Wall -o tcp_client tcp_client.c socket_utils.c
sudo ./tcp_client
```

**Notice**: You must run the server before the client. If you're going to use the tcp server, you must also use the tcp client.
# Load Test
Run the server normally, compile the client, then compile and run the load test script as follows:
```
gcc -Wall -o load_test load_test.c
load_test 50 100 client
```
This will run the load test with 50 clients and 100 requests for each client using `client` as the binary (you could also use tcp client and server).
