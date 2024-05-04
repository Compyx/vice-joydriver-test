/** \file   lib.c
 * \brief   Helper functions, mimicking some of VICE's lib.c
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "lib.h"


extern bool verbose;


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
    rlen = strlen(s);
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


char *lib_msprintf(const char *fmt, ...)
{
    char    *s;
    va_list  ap;
    size_t   size;
    int      len;

    /* determine required size */
    va_start(ap, fmt);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) {
        return NULL;
    }

    /* allocate appropriate size */
    size = (size_t)len + 1u;
    s = lib_malloc(size);

    va_start(ap, fmt);
    len = vsnprintf(s, size, fmt, ap);
    va_end(ap);
    return s;
}


/** \brief  Right-trim string
 *
 * Remove trailing whitespace from string \a s, replacing each instance with
 * \c 0.
 *
 * \param[in,out]   s   string to trim
 */
void lib_strrtrim(char *s)
{
    if (s != NULL && *s != '\0') {
        long i = (long)strlen(s) - 1;

        while (i >= 0 && isspace((unsigned char)s[i])) {
            s[i--] = '\0';
        }
    }
}


const char *util_skip_whitespace(const char *s)
{
    if (s != NULL) {
        while (*s != '\0' && isspace((unsigned char)*s)) {
            s++;
        }
    }
    return s;
}


#ifdef WINDOWS_COMPILE
#define ARCHDEP_DIR_SEP_CHR '\\'
#else
#define ARCHDEP_DIR_SEP_CHR '/'
#endif

const char *lib_basename(const char *s)
{
    const char *t;

    if (s == NULL || *s == '\0') {
        return s;
    }

    t = s + strlen(s) - 1;
    while (t >= s && *t != ARCHDEP_DIR_SEP_CHR) {
        t--;
    }
    return t + 1;
}
