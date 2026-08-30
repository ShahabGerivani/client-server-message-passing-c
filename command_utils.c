#include "command_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_BUF_SIZE 512

/**
 * Helper function to safely execute a command via popen and read its stdout stream.
 */
static char *read_command_output(const char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        return NULL;
    }

    size_t capacity = INITIAL_BUF_SIZE;
    size_t length = 0;
    char *buffer = malloc(capacity);
    if (buffer == NULL) {
        pclose(fp);
        return NULL;
    }

    char temp[256];
    while (fgets(temp, sizeof(temp), fp) != NULL) {
        size_t len = strlen(temp);
        if (length + len + 1 > capacity) {
            capacity *= 2;
            while (length + len + 1 > capacity) {
                capacity *= 2;
            }
            char *new_buf = realloc(buffer, capacity);
            if (new_buf == NULL) {
                free(buffer);
                pclose(fp);
                return NULL;
            }
            buffer = new_buf;
        }
        memcpy(buffer + length, temp, len);
        length += len;
    }

    buffer[length] = '\0';
    pclose(fp);
    return buffer;
}

char *get_ls(const char *path) {
    if (path == NULL) {
        return read_command_output("ls");
    }

    /* Allocate buffer for "ls -- " + path string */
    size_t cmd_len = strlen(path) + 8;
    char *cmd = malloc(cmd_len);
    if (cmd == NULL) {
        return NULL;
    }

    snprintf(cmd, cmd_len, "ls -- %s", path);
    char *result = read_command_output(cmd);
    free(cmd);
    return result;
}

char *get_pwd(void) {
    return read_command_output("pwd");
}

char *get_cat(const char *filepath) {
    if (filepath == NULL) {
        return NULL;
    }

    /* Allocate buffer for "cat -- " + filepath string */
    size_t cmd_len = strlen(filepath) + 9;
    char *cmd = malloc(cmd_len);
    if (cmd == NULL) {
        return NULL;
    }

    snprintf(cmd, cmd_len, "cat -- %s", filepath);
    char *result = read_command_output(cmd);
    free(cmd);
    return result;
}
