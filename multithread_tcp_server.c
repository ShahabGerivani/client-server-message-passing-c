#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#include "socket_utils.h"
#include "request.h"
#include "command_utils.h"
#include "socket_address.h"

/*
 * Convert socket to IP address string.
 * addr: struct sockaddr_in or struct sockaddr_in6
 */
const char *inet_ntop2(void *addr, char *buf, size_t size)
{
    struct sockaddr_storage *sas = addr;
    struct sockaddr_in *sa4;
    struct sockaddr_in6 *sa6;
    void *src;

    switch (sas->ss_family) {
        case AF_INET:
            sa4 = addr;
            src = &(sa4->sin_addr);
            break;
        case AF_INET6:
            sa6 = addr;
            src = &(sa6->sin6_addr);
            break;
        default:
            return NULL;
    }

    return inet_ntop(sas->ss_family, src, buf, size);
}

/*
 * Return a listening socket.
 */
int get_listener_socket(void)
{
    int listener;     // Listening socket descriptor
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo *ai, *p;
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE
    };

    // Get us a socket and bind it
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "pollserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // If we got here, it means we didn't get bound
    if (p == NULL) {
        return -1;
    }

    freeaddrinfo(ai); // All done with this

    // Listen
    if (listen(listener, 10) == -1) {
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
        struct sockaddr_storage remoteaddr; // Client address
        socklen_t addrlen;
        int *newfd = malloc(sizeof(int));
        if (!newfd) {
            printf("Memory allocation error for newfd!\n");
            exit(1);
        }
        char remoteIP[INET6_ADDRSTRLEN];

        addrlen = sizeof remoteaddr;
        *newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);
        if (*newfd == -1) {
            perror("accept");
            free(newfd);
            continue;
        }
        printf("New connection from %s on socket %d\n", inet_ntop2(&remoteaddr, remoteIP, sizeof remoteIP), *newfd);

        pthread_t threadID;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&threadID, &attr, process_client, newfd); // Ownership of newfd transfers to process_client
        pthread_attr_destroy(&attr);
	}
}
