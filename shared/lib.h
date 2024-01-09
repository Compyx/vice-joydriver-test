/** \file   lib.h
 * \brief   Helper functions, mimicking some of VICE's lib.c - header
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */


#ifndef SHARED_LIB_H
#define SHARED_LIB_H

#include <stddef.h>
#include <stdbool.h>

#define ARRAY_LEN(arr)  (sizeof arr / sizeof arr[0])

void *lib_malloc(size_t size);
void *lib_realloc(void *ptr, size_t size);
void  lib_free(void *ptr);

char *lib_strdup(const char *s);
char *lib_strndup(const char *s, size_t n);

#endif
