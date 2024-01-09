/** \file   lib.c
 * \brief   Helper functions, mimicking some of VICE's lib.c
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "lib.h"


void *lib_malloc(size_t size)
{
    void *ptr = malloc(size);

    if (ptr == NULL) {
        fprintf(stderr,
                "%s(): fatal: failed to allocate %zu bytes.",
                __func__, size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}


void *lib_realloc(void *ptr, size_t size)
{
    void *tmp = realloc(ptr, size);

    if (tmp == NULL) {
         fprintf(stderr,
                "%s(): fatal: failed to allocate %zu bytes.",
                __func__, size);
         exit(EXIT_FAILURE);
    }
    return tmp;
}


void lib_free(void *ptr)
{
    free(ptr);
}


char *lib_strdup(const char *s)
{
    char *t;

    if (s == NULL || *s == '\0') {
        t = malloc(1u);
        *t = '\0';
    } else {
        size_t len = strlen(s);

        t = lib_malloc(len + 1u);
        memcpy(t, s, len + 1u);
    }
    return t;
}


char *lib_strndup(const char *s, size_t n)
{
    char *t;

    if (s == NULL || n == 0) {
        t = lib_strdup(NULL);
    } else {
        size_t len = strlen(s);

        if (len > n) {
            t = lib_malloc(n + 1u);
            memcpy(t, s, n);
            t[n] = '\0';
        } else {
            t = lib_malloc(len + 1u);
            memcpy(t, s, len + 1u);
        }
    }
    return t;
}
