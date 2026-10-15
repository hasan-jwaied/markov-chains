#ifndef MARKOV_CLI_COMMON_H
#define MARKOV_CLI_COMMON_H

#include <stdbool.h>
#include <stddef.h>

/** Parse a base-10 integer in [0, max]. Prints an error naming `what`. */
bool cli_parse_ulong(const char *str, unsigned long max, const char *what,
                     unsigned long *out);

/**
 * Read a whole file into a malloc'd buffer (NUL-terminated for convenience;
 * *len excludes the terminator). Returns false if it cannot be read.
 */
bool cli_read_file(const char *path, char **data, size_t *len);

#endif /* MARKOV_CLI_COMMON_H */
