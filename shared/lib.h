/** \file   lib.h
 * \brief   Helper functions, mimicking some of VICE's lib.c - header
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */


#ifndef SHARED_LIB_H
#define SHARED_LIB_H

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

extern bool debug;
extern bool verbose;

#define ARRAY_LEN(arr)  (sizeof arr / sizeof arr[0])

#define msg_verbose(...) \
    if (verbose) { \
        printf(__VA_ARGS__); \
    }

#define msg_debug(...) \
    if (debug) { \
        printf("[DBG: %s:%d:%s()] ", __FILE__, __LINE__, __func__); \
        printf(__VA_ARGS__); \
    }

#define msg_error(...) \
    fprintf(stderr, "%s(): error: ", __func__); \
    fprintf(stderr, __VA_ARGS__);

void *lib_malloc(size_t size);
void *lib_realloc(void *ptr, size_t size);
void  lib_free(void *ptr);

char *lib_strdup(const char *s);
char *lib_strndup(const char *s, size_t n);
char *util_concat(const char *s, ...);

#endif
