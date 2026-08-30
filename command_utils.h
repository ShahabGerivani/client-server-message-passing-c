#ifndef UTILS_COMMAND_H
#define UTILS_COMMAND_H

/**
 * Executes 'ls' on the target directory path and returns the output as a dynamically allocated string.
 * Caller is responsible for freeing the returned memory using free().
 * Returns NULL on failure.
 */
char *get_ls(const char *path);

/**
 * Executes 'pwd' and returns the current working directory as a dynamically allocated string.
 * Caller is responsible for freeing the returned memory using free().
 * Returns NULL on failure.
 */
char *get_pwd(void);

/**
 * Executes 'cat' on the specified absolute file path and returns its contents as a dynamically allocated string.
 * Caller is responsible for freeing the returned memory using free().
 * Returns NULL on failure.
 */
char *get_cat(const char *filepath);

#endif /* UTILS_COMMAND_H */
