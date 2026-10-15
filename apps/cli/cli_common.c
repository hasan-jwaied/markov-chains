#include "cli_common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

bool cli_parse_ulong(const char *str, unsigned long max, const char *what,
                     unsigned long *out)
{
    char *end = NULL;
    errno = 0;
    /* strtoul accepts a leading '-' and negates; reject it explicitly. */
    const char *p = str;
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }
    unsigned long value = strtoul(str, &end, 10);
    if (*p == '-' || errno != 0 || end == str || *end != '\0' || value > max)
    {
        fprintf(stderr, "Error: invalid value for %s: '%s'\n", what, str);
        return false;
    }
    *out = value;
    return true;
}

bool cli_read_file(const char *path, char **data, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        return false;
    }
    size_t cap = 1u << 16u;
    size_t used = 0;
    char *buf = malloc(cap);
    while (buf != NULL)
    {
        used += fread(buf + used, 1, cap - used - 1u, fp);
        if (used < cap - 1u)
        {
            break; /* EOF or error */
        }
        char *grown = realloc(buf, cap * 2u);
        if (grown == NULL)
        {
            free(buf);
            buf = NULL;
            break;
        }
        buf = grown;
        cap *= 2u;
    }
    bool ok = buf != NULL && !ferror(fp);
    fclose(fp);
    if (!ok)
    {
        free(buf);
        return false;
    }
    buf[used] = '\0';
    *data = buf;
    *len = used;
    return true;
}
