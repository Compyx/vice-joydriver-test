/** \file   lib.c
 * \brief   Helper functions, mimicking some of VICE's lib.c
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

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


/** \brief  Create heap-allocated copy of string of at most N bytes
 * \param[in]   s   string
 * \param[in]   n   maximum number of characters to copy of \a s
 *
 * \return  heap-allocated string, free with \c lib_free()
 * \note    Unlike \c strncpy(3) this function does add a terminating \c nul
 *          character.
 */
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

/** \brief  Concatenate a list of strings into a new string
 *
 * \param[in]   s   list of strings to join, terminate with \a NULL
 *
 * \return  heap-allocated new string, free with \c lib_free()
 */
char *util_concat(const char *s, ...)
{
    char       *result;
    char       *rpos;
    size_t      rlen;
    const char *arg;
    size_t      alen;
    va_list     ap;

    va_start(ap, s);
    rlen = 0;
    while ((arg = va_arg(ap, const char *)) != NULL) {
        rlen += strlen(arg);
    }
    va_end(ap);

    result = lib_malloc(rlen + 1u);
    alen = strlen(s);
    memcpy(result, s, alen);
    rpos = result + alen;

    va_start(ap, s);
    while ((arg = va_arg(ap, const char *)) != NULL) {
        alen = strlen(arg);
        if (alen > 0) {
            memcpy(rpos, arg, alen);
            rpos += alen;
        }
    }
    *rpos = '\0';
    return result;
}
