/** \file   joymap.c
 * \brief   VICE joymap file (*.vjm) parsing
 *
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>

#include "lib.h"

#include "joymap.h"


#define VJM_COMMENT '#'

#define LINEBUF_INITIAL_SIZE    256

typedef enum {
    VJM_KW_INVALID = -1,
    VJM_KW_VJM_VERSION,
    VJM_KW_DEV_NAME,
    VJM_KW_DEV_VENDOR,
    VJM_KW_DEV_PRODUCT,
    VJM_KW_DEV_VERSION,
    VJM_KW_MAP_PIN,
    VJM_KW_MAP_POT,
    VJM_KW_MAP_KEY,
    VJM_KW_MAP_ACTION
} keyword_id_t;


static const char *keywords[] = {
    "vjm-version",
    "device-name", "device-vendor", "device-product", "device-version",
    "pin", "pot", "key", "action"
};


static char   *linebuf;
static size_t  linebuf_size;
static size_t  linebuf_len;
static int     linenum;
char          *lineptr;
const char    *current_path;


/** \brief  Strip trailing whitespace from line buffer */
static void rtrim_linebuf(void)
{
    char *p = linebuf + linebuf_len - 1;

    while (p >= linebuf && isspace((unsigned char)*p)) {
        *p-- = '\0';
    }
}

static void skip_whitespace(void)
{
    while (*lineptr != '\0' && isspace((unsigned char)*lineptr)) {
        lineptr++;
    }
}


static void parser_log_helper(const char *prefix, const char *fmt, va_list args)
{
    char    msg[1024];

    vsnprintf(msg, sizeof msg, fmt, args);
    if (prefix != NULL) {
        fprintf(stderr, "%s: ", prefix);
    }
    fprintf(stderr,
            "%s:%d:%d: %s\n",
            lib_basename(current_path), linenum, (int)(lineptr - linebuf) + 1, msg);
}

static void parser_log_error(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    parser_log_helper("error", fmt, args);
    va_end(args);
}

static void parser_log_warning(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    parser_log_helper("warning", fmt, args);
    va_end(args);
}


static joymap_t *joymap_new(void)
{
    joymap_t *joymap = lib_malloc(sizeof *joymap);

    joymap->path        = NULL;
    joymap->fp          = NULL;
    joymap->ver_major   = 0;
    joymap->ver_minor   = 0;
    joymap->dev_name    = NULL;
    joymap->dev_vendor  = 0x0000;
    joymap->dev_product = 0x0000;
    joymap->dev_version = 0x0000;

    return joymap;
}


void joymap_free(joymap_t *joymap)
{
    if (joymap->fp != NULL) {
        fclose(joymap->fp);
    }
    lib_free(joymap->path);
    lib_free(joymap->dev_name);
    lib_free(joymap);
}


static joymap_t *joymap_open(const char *path)
{
    joymap_t *joymap;
    FILE     *fp;

    linenum    = 1;
    linebuf[0] = '\0';

    fp = fopen(path, "r");
    if (fp == NULL) {
        msg_error("failed to open vjm file for reading: %s\n", strerror(errno));
        return NULL;
    }

    joymap = joymap_new();
    joymap->fp   = fp;
    joymap->path = lib_strdup(path);
    return joymap;
}


static bool joymap_read_line(joymap_t *joymap)
{
    linebuf_len = 0;
    linebuf[0]  = '\0';

    while (true) {
        int ch;

        if (linebuf_len - 1u == linebuf_size) {
            linebuf_size *= 2u;
            linebuf = lib_realloc(linebuf, linebuf_size);
        }

        ch = fgetc(joymap->fp);
        if (ch == EOF) {
            linebuf[linebuf_len] = '\0';
            rtrim_linebuf();
            return false;
        } else if (ch == '\n') {
            break;
        }
        linebuf[linebuf_len++] = (char)ch;
    }

    linebuf[linebuf_len] = '\0';
    rtrim_linebuf();
    linenum++;
    return true;
}

static char *get_quoted_arg(char **endptr)
{
    char *pos;
    bool  escaped;

    printf("lineptr = %s\n", lineptr);

    if (*lineptr != '"') {
        if (endptr != NULL) {
            *endptr = lineptr;
        }
        parser_log_error("expected opening quote");
        return NULL;
    }

    escaped = false;
    pos     = lineptr + 1;
    while (*pos != '\0') {
        if (*pos == '"') {
            if (!escaped) {
                /* end of argument */
                if (endptr != NULL) {
                    *endptr = pos;
                }
                return lib_strndup(lineptr + 1, (size_t)(pos - lineptr - 1));
            } else {
                pos++;
            }
        } else if (*pos == '\\') {
            if (escaped) {
                pos++;
            } else {
                escaped = true;
            }
        } else {
            pos++;
        }
    }

    parser_log_error("unterminated string literal");
    return NULL;
}

static bool get_int_arg(int *value, char **endptr)
{
    long result;

    errno = 0;
    result = strtol(lineptr, endptr, 0);
    if (errno != 0) {
        parser_log_error("failed to convert '%s' to long", lineptr);
        return false;
    }
    msg_debug("got value %ld\n", result);
#if (INT_MAX < LONG_MAX)
    if (value > INT_MAX) {
        parser_log_error("value %ld out of range for int", value);
        return false;
    }
#endif
#if (INT_MIN > LONG_MIN)
    if (value < INT_MIN) {
        parser_log_error("value %ld out of range for int", value);
        return false;
    }
#endif

    *value  = (int)result;
    return true;
}


static bool get_vjm_version(joymap_t *joymap)
{
    int   major;
    int   minor;
    char *endptr;

    if (!get_int_arg(&major, &endptr)) {
        parser_log_error("expected major version number");
        return false;
    }
    if (*endptr != '.') {
        parser_log_error("expected dot after major version number");
        return false;
    }
    lineptr = endptr + 1;
    if (!get_int_arg(&minor, &endptr)) {
        parser_log_error("expected minor version number");
        return false;
    }
    joymap->ver_major = major;
    joymap->ver_minor = minor;
    return true;
}


static bool handle_keyword(joymap_t *joymap, keyword_id_t kw)
{
    int   vendor;
    int   product;
    char *endptr;

    if (*lineptr == '\0') {
        parser_log_error("missing data after keyword '%s'", keywords[kw]);
        return false;
    }

    switch (kw) {
        case VJM_KW_VJM_VERSION:
            if (!get_vjm_version(joymap)) {
                return false;
            }
            msg_debug("VJM version: %d.%d\n", joymap->ver_major, joymap->ver_minor);
            break;

        case VJM_KW_DEV_VENDOR:
            if (!get_int_arg(&vendor, &endptr)) {
                return false;
            }
            if (vendor < 0 || vendor > 0xffff) {
                parser_log_error("illegal value %d for device vendor ID", vendor);
                return false;
            }
            joymap->dev_vendor = (uint16_t)vendor;
            break;

        case VJM_KW_DEV_PRODUCT:
            if (!get_int_arg(&product, &endptr)) {
                return false;
            }
            if (product < 0 || product > 0xffff) {
                parser_log_error("illegal value %d for device product ID", product);
                return false;
            }
            joymap->dev_product = (uint16_t)product;
            break;

        default:
            parser_log_warning("keyword '%s' is not implemented yet", keywords[kw]);
            break;
    }
    return true;
}


static bool joymap_parse_line(joymap_t *joymap)
{
    char         *pos;
    size_t        i;
    keyword_id_t  kw;

    lineptr = linebuf;

    skip_whitespace();
    if (*lineptr == '\0' || *lineptr == VJM_COMMENT) {
        return true;
    }
    msg_debug("parsing line %d: \"%s\"\n", linenum, lineptr);

    /* get keyword */
    pos = lineptr;
    while (*pos != '\0' && !isspace((unsigned char)*pos)) {
        pos++;
    }
    kw = VJM_KW_INVALID;
    for (i = 0; i < ARRAY_LEN(keywords); i++) {
        if (strncmp(keywords[i], lineptr, (size_t)(pos - lineptr)) == 0) {
            kw = (keyword_id_t)i;
            break;
        }
    }
    if (kw == VJM_KW_INVALID) {
        *pos = '\0';
        parser_log_error("unknown keyword '%s'", lineptr);
        return false;
    }
    printf("found keyword: %d: %s\n", (int)kw, keywords[kw]);
    lineptr = pos;
    skip_whitespace();

    return handle_keyword(joymap, kw);
}


joymap_t *joymap_load(const char *path)
{
    joymap_t *joymap = NULL;

    current_path = path;

    msg_debug("loading joymap file '%s'\n", path);
    joymap = joymap_open(path);
    if (joymap == NULL) {
        return NULL;
    }

    errno   = 0;
    linenum = 0;
    while (joymap_read_line(joymap)) {
        if (!joymap_parse_line(joymap)) {
            joymap_free(joymap);
            return NULL;
        }
    }
    if (errno != 0) {
        joymap_free(joymap);
        joymap = NULL;
    }
    return joymap;
}


void joymap_module_init(void)
{
    linebuf_size = LINEBUF_INITIAL_SIZE;
    linebuf_len  = 0;
    linebuf      = lib_malloc(linebuf_size);
    linebuf[0]   = '\0';
}


void joymap_module_shutdown(void)
{
    lib_free(linebuf);
}
