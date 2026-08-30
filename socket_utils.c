#include "socket_utils.h"

#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

ssize_t send_all(int fd, const void *buf, size_t len)
{
    size_t total = 0;
    const char *p = buf;

    while (total < len) {
        ssize_t n = send(fd, p + total, len - total, 0);

        if (n > 0) {
            total += n;
        } else if (n == 0) {
            // send() returning 0 with len > 0 is unusual
            break;
        } else {
            if (errno == EINTR)
                continue;

            return -1;
        }
    }

    return total;
}

ssize_t recv_all(int fd, void *buf, size_t len)
{
    size_t total = 0;
    char *p = buf;

    while (total < len) {
        ssize_t n = recv(fd, p + total, len - total, 0);

        if (n > 0) {
            total += n;
        } else if (n == 0) {
            // Peer closed the connection
            break;
        } else {
            if (errno == EINTR)
                continue;

            return -1;
        }
    }

    return total;
}
