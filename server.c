#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <sys/un.h>

#include "socket_utils.h"
#include "request.h"
#include "queue.h"
#include "command_utils.h"
#include "socket_address.h"

#define MAX_REQS_TO_PROCESS 10 // Maximum number of requests that we would process before handling sockets again

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
 * Add a new file descriptor to the set.
 */
void add_to_pfds(struct pollfd **pfds, int newfd, int *fd_count,
		int *fd_size)
{
	// If we don't have room, add more space in the pfds array
	if (*fd_count == *fd_size) {
		*fd_size *= 2; // Double it
		*pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
	}

	(*pfds)[*fd_count].fd = newfd;
	(*pfds)[*fd_count].events = POLLIN; // Check ready-to-read
	(*pfds)[*fd_count].revents = 0;

	(*fd_count)++;
}

/*
 * Remove a file descriptor at a given index from the set.
 */
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
	// Copy the one from the end over this one
	pfds[i] = pfds[*fd_count-1];

	(*fd_count)--;
}

/*
 * Handle incoming connections.
 */
void handle_new_connection(int listener, int *fd_count,
		int *fd_size, struct pollfd **pfds)
{
	struct sockaddr_un remote = {0}; // Client address
	socklen_t addrlen;
	int newfd;  // Newly accept()ed socket descriptor

	addrlen = sizeof remote;
	newfd = accept(listener, (struct sockaddr *)&remote, &addrlen);

	if (newfd == -1) {
		perror("accept");
	} else {
		add_to_pfds(pfds, newfd, fd_count, fd_size);

		printf("server: new connection from %s on socket %d\n",
				remote.sun_path,
				newfd);
	}
}


void close_connection(int *fd_count, struct pollfd *pfds, int *pfd_i) {
    close(pfds[*pfd_i].fd); // Bye!
    del_from_pfds(pfds, *pfd_i, fd_count);
    // reexamine the slot we just deleted
    (*pfd_i)--;
}


/*
 * Handle regular client data or client hangups.
 */
void add_request_to_queue(int *fd_count, struct pollfd *pfds, int *pfd_i, struct queue *req_queue)
{
    // Getting data from the socket
    uint16_t req_len;
    if (recv_all(pfds[*pfd_i].fd, &req_len, sizeof req_len) <= 0) {
        perror("recv_all(req_len)");
        close_connection(fd_count, pfds, pfd_i);
        return;
    }

    uint16_t req_type;
    if (recv_all(pfds[*pfd_i].fd, &req_type, sizeof req_type) <= 0) {
        perror("recv_all(req_type)");
        close_connection(fd_count, pfds, pfd_i);
        return;
    }

    uint16_t req_arg_len;
    if (recv_all(pfds[*pfd_i].fd, &req_arg_len, sizeof req_arg_len) <= 0) {
        perror("recv_all(req_arg_len)");
        close_connection(fd_count, pfds, pfd_i);
        return;
    }

    char *req_arg = malloc(req_arg_len);
    if (!req_arg) {
        printf("Memory allocation error for req_arg!\n");
        return;
    }
    if (recv_all(pfds[*pfd_i].fd, req_arg, req_arg_len) <= 0) {
        perror("recv_all(req_arg)");
        close_connection(fd_count, pfds, pfd_i);
        return;
    }

    // Create the request from recieved data
    struct request req = {
        .client_fd = pfds[*pfd_i].fd,
        .req_arg_len = req_arg_len,
        .req_arg = req_arg
    };
    if (!request_type_from_wire(req_type, &(req.req_type))) {
        printf("Error converting req_type\n");
        free(req_arg);
        return;
    }

    // Add request to queue
    queue_push(req_queue, &req);
}

/*
 * Process all existing connections.
 */
void process_connections(int listener, int *fd_count, int *fd_size,
		struct pollfd **pfds, struct queue *req_queue)
{
	for(int i = 0; i < *fd_count; i++) {

		// Check if someone's ready to read
		if ((*pfds)[i].revents & (POLLIN | POLLHUP)) {
			// We got one!!

			if ((*pfds)[i].fd == listener) {
				// If we're the listener, it's a new connection
				handle_new_connection(listener, fd_count, fd_size,
						pfds);
			} else {
				// Otherwise we're just a regular client
                printf("Request from socket %d\n", (*pfds)[i].fd);
				add_request_to_queue(fd_count, *pfds, &i, req_queue);
			}
		}
	}
}

void process_requests(struct queue *req_queue) {
    for (int i = 0; !queue_is_empty(req_queue) && i < MAX_REQS_TO_PROCESS; i++) {
        struct request req;
        queue_pop(req_queue, &req);

        char *response_data;
        switch (req.req_type)
        {
        case REQUEST_TYPE_CAT:
            response_data = get_cat(req.req_arg);
            break;
        case REQUEST_TYPE_LS:
            response_data = get_ls(req.req_arg);
            break;
        case REQUEST_TYPE_PWD:
            response_data = get_pwd();
            break;
        }

        uint16_t response_len = sizeof(uint16_t) + strlen(response_data) + 1;
        if (send_all(req.client_fd, &response_len, sizeof response_len) > 0) {
            if (send_all(req.client_fd, response_data, strlen(response_data) + 1) <= 0) {
                perror("send_all(response_data)");
            }
        } else {
            perror("send_all(len)");
        }

        free(response_data);
        free(req.req_arg);
    }
}

/*
 * Main: create a listener and connection set, loop forever
 * processing connections.
 */
int main(void)
{
	int listener;	 // Listening socket descriptor

	// Start off with room for 5 connections
	// (We'll realloc as necessary)
	int fd_size = 5;
	int fd_count = 0;
	struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

	// Set up and get a listening socket
	listener = get_listener_socket();

	if (listener == -1) {
		fprintf(stderr, "error getting listening socket\n");
		exit(1);
	}

	// Add the listener to set;
	// Report ready to read on incoming connection
	pfds[0].fd = listener;
	pfds[0].events = POLLIN;

	fd_count = 1; // For the listener
	
	puts("server: waiting for connections...");

    // The request queue
    struct queue req_queue;
    if (queue_init(&req_queue, 0, true) < 0) {
        printf("Error initializing queue");
        exit(1);
    }

	// Main loop
	for(;;) {
		int poll_count = poll(pfds, fd_count, -1);

		if (poll_count == -1) {
			perror("poll");
			exit(1);
		}

		// Run through connections looking for data to read
		process_connections(listener, &fd_count, &fd_size, &pfds, &req_queue);
            
        process_requests(&req_queue);
	}

	free(pfds);
}
