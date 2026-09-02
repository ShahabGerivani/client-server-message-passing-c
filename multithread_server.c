#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#include "socket_utils.h"
#include "request.h"
#include "command_utils.h"
#include "socket_address.h"

/*
 * Return a listening socket.
 */
int get_listener_socket(void)
{
    struct sockaddr_un local = {
        .sun_family = AF_UNIX,
    };
    strcpy(local.sun_path, SOCKET_PATH);
    unlink(local.sun_path);

    int listener;
    if ((listener = socket(local.sun_family, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    if (bind(listener, (struct sockaddr *)&local, sizeof(local)) < 0) {
        close(listener);
        perror("bind");
        return -1;
    }

    if (listen(listener, 10) < 0) {
        perror("listen");
        return -1;
    }
    return listener;
}

/*
 * Process a client in its own thread
 */
void *process_client(void *arg) {
    int fd = *(int*)arg;

    while (1) {
        // Getting data from the socket
        uint16_t req_len;
        if (recv_all(fd, &req_len, sizeof req_len) <= 0) {
            perror("recv_all(req_len)");
            break;
        }

        uint16_t req_type;
        if (recv_all(fd, &req_type, sizeof req_type) <= 0) {
            perror("recv_all(req_type)");
            break;
        }

        uint16_t req_arg_len;
        if (recv_all(fd, &req_arg_len, sizeof req_arg_len) <= 0) {
            perror("recv_all(req_arg_len)");
            break;
        }

        char req_arg[req_arg_len];
        if (recv_all(fd, req_arg, req_arg_len) <= 0) {
            perror("recv_all(req_arg)");
            break;
        }

        // Running the command
        char *response_data;
        switch (req_type)
        {
        case REQUEST_TYPE_CAT:
            response_data = get_cat(req_arg);
            break;
        case REQUEST_TYPE_LS:
            response_data = get_ls(req_arg);
            break;
        case REQUEST_TYPE_PWD:
            response_data = get_pwd();
            break;
        }

        // Sending back the results
        uint16_t response_len = sizeof(uint16_t) + strlen(response_data) + 1;
        if (send_all(fd, &response_len, sizeof response_len) > 0) {
            if (send_all(fd, response_data, strlen(response_data) + 1) <= 0) {
                perror("send_all(response_data)");
            }
        } else {
            perror("send_all(len)");
        }
        free(response_data);
    }

    free(arg);
    close(fd);
    pthread_exit(NULL);
}

/*
 * Main: create a listener, loop forever creating threads for clients.
 */
int main(void)
{
    // Set up and get a listening socket
	int listener;
	listener = get_listener_socket();
	if (listener == -1) {
		fprintf(stderr, "error getting listening socket\n");
		exit(1);
	}
	
	puts("server: waiting for connections...");

	// Main loop
	for(;;) {
        struct sockaddr_un remote = {0}; // Client address
        socklen_t addrlen;
        int *newfd = malloc(sizeof(int));
        if (!newfd) {
            printf("Memory allocation error for newfd!\n");
            exit(1);
        }
        addrlen = sizeof remote;
        *newfd = accept(listener, (struct sockaddr *)&remote, &addrlen);
        if (*newfd == -1) {
            perror("accept");
            close(newfd);
            continue;
        }
        printf("New connection from %s on socket %d\n", remote.sun_path, *newfd);

        pthread_t threadID;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&threadID, &attr, process_client, newfd); // Ownership of newfd transfers to process_client
        pthread_attr_destroy(&attr);
	}
}
