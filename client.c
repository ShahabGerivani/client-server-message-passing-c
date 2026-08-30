#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>

#include "socket_address.h"
#include "request.h"
#include "socket_utils.h"

#define MAX_COMMAND_LEN 3
#define MAX_ARG_LEN 127
#define MAX_LINE_LEN 130

// Helper macros to convert numbers into string literals
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
// Input format string
#define FMT_STRING "%" STR(MAX_COMMAND_LEN) "s %" STR(MAX_ARG_LEN) "[^\n]"

int main(int argc, char const *argv[])
{
    int s, len;
    struct sockaddr_un remote = {
        .sun_family = AF_UNIX,
    };

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    printf("Trying to connect...\n");

    strcpy(remote.sun_path, SOCKET_PATH);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(s, (struct sockaddr *)&remote, len) == -1) {
        perror("connect");
        exit(1);
    }

    printf("Connected.\n");

    uint16_t req_type = 67, arg_len = MAX_ARG_LEN, req_len = arg_len + 6;
    while (1)
    {
        char line[MAX_LINE_LEN + 1] = "", command[MAX_COMMAND_LEN + 1] = "", arg[MAX_ARG_LEN + 1] = "";

        printf("Enter your command: ");
        fgets(line, MAX_LINE_LEN, stdin);

        int input_num = sscanf(line, FMT_STRING, command, arg);

        if (strcmp(command, "ls") == 0) req_type = REQUEST_TYPE_LS;
        else if (strcmp(command, "pwd") == 0) req_type = REQUEST_TYPE_PWD;
        else if (strcmp(command, "cat") == 0) req_type = REQUEST_TYPE_CAT;
        else {
            printf("The command should be ls, pwd, or cat \n");
            continue;            
        }

        switch (req_type)
        {
        case REQUEST_TYPE_LS:
            if (input_num < 2) {
                printf("You should provide a path for ls\n");
                continue;
            }
            break;
        case REQUEST_TYPE_CAT:
            if (input_num < 2) {
                printf("You should provide an absolute filename for cat\n");
                continue;
            }
            break;
        default:
            if (input_num < 1) {
                printf("Please type in a command!\n");
                continue;
            }
            break;
        }

        // Sending the request
        if (send_all(s, &req_len, sizeof req_len) <= 0) {
            perror("send_all(req_len)");
            continue;
        }
        if (send_all(s, &req_type, sizeof req_type) <= 0) {
            perror("send_all(req_type)");
            continue;
        }
        if (send_all(s, &arg_len, sizeof arg_len) <= 0) {
            perror("send_all(arg_len)");
            continue;
        }
        if (send_all(s, arg, arg_len) <= 0) {
            perror("send_all(arg)");
            continue;
        }

        // Recieving and printing the response
        uint16_t response_len;
        if (recv_all(s, &response_len, sizeof response_len) <= 0) {
            perror("recv_all(response_len)");
            continue;
        }

        char *response_data = malloc(response_len - sizeof(response_len));
        if (recv_all(s, response_data, response_len - sizeof(response_len)) <= 0) {
            free(response_data);
            perror("recv_all(response_data)");
            continue;
        }

        printf("server: %s \n", response_data);
        free(response_data);
    }
    
    return 0;
}
