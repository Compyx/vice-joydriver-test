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

/** \brief  Initial size of mappings array in a joymap object
 *
 * Number of elements initially allocated for the `mappings` array
 */
#define MAPPINGS_INITIAL_SIZE   16

typedef enum {
    VJM_KW_INVALID = -1,
    VJM_KW_VJM_VERSION,
    VJM_KW_DEVICE_NAME,
    VJM_KW_DEVICE_VENDOR,
    VJM_KW_DEVICE_PRODUCT,
    VJM_KW_DEVICE_VERSION,
    VJM_KW_MAP,
    VJM_KW_PIN,
    VJM_KW_POT,
    VJM_KW_KEY,
    VJM_KW_ACTION,
    VJM_KW_NONE,
    VJM_KW_AXIS,
    VJM_KW_BUTTON,
    VJM_KW_HAT,
    VJM_KW_NORTH,
    VJM_KW_NORTHEAST,
    VJM_KW_EAST,
    VJM_KW_SOUTHEAST,
    VJM_KW_SOUTH,
    VJM_KW_SOUTHWEST,
    VJM_KW_WEST,
    VJM_KW_NORTHWEST
} keyword_id_t;


static const char *keywords[] = {
    "vjm-version",
    "device-name", "device-vendor", "device-product", "device-version",
    "map",
    "pin", "pot", "key", "action", "none",
    "axis", "button", "hat",
    "north", "northeast", "east", "southeast", "south", "southwest", "west", "northwest"
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
    joymap->ver_minor     = 0;
    joymap->dev_name      = NULL;
    joymap->dev_vendor    = 0x0000;
    joymap->dev_product   = 0x0000;
    joymap->dev_version   = 0x0000;
    joymap->mappings_size = MAPPINGS_INITIAL_SIZE;
    joymap->mappings_num  = 0;
    joymap->mappings      = lib_malloc(sizeof *(joymap->mappings) * MAPPINGS_INITIAL_SIZE);

    return joymap;
}


void joymap_free(joymap_t *joymap)
{
    if (joymap->fp != NULL) {
        fclose(joymap->fp);
    }
    lib_free(joymap->path);
    lib_free(joymap->dev_name);
    lib_free(joymap->mappings);
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

static keyword_id_t get_keyword(char **endptr)
{
    keyword_id_t  id  = VJM_KW_INVALID;
    char         *pos = lineptr;

    /* keyword  = '[a-zA-Z\-]' */
    while (*pos != '\0' && (isalpha((unsigned char)*pos) || *pos == '-')) {
        pos++;
    }
    if (pos > lineptr) {
        size_t i;

        for (i = 0; i < ARRAY_LEN(keywords); i++) {
            if (strncmp(keywords[i], lineptr, (size_t)(pos - lineptr)) == 0) {
                id = (keyword_id_t)i;
                break;
            }
        }
    }
    if (endptr != NULL) {
        *endptr = pos;
    }
    return id;
}

static bool get_quoted_arg(char **value, char **endptr)
{
    char *result;
    char *rpos;
    char *lpos;
    bool  escaped;

    if (*lineptr != '"') {
        if (endptr != NULL) {
            *endptr = lineptr;
        }
        parser_log_error("expected opening double quote");
        *value = NULL;
        return false;
    }

    result  = lib_malloc(linebuf_len - (size_t)(lineptr - linebuf) + 1u);
    escaped = false;
    lpos    = lineptr + 1;
    rpos    = result;
    while (*lpos != '\0') {
        if (*lpos == '"') {
            if (!escaped) {
                /* end of argument */
                if (endptr != NULL) {
                    *endptr = lpos;
                }
                *rpos = '\0';
                *value = result;
                return true;
            } else {
                *rpos++ = *lpos;
                escaped = false;
            }
        } else if (*lpos == '\\') {
            if (escaped) {
                *rpos++ = *lpos;
                escaped = false;
            } else {
                escaped = true;
            }
        } else {
            *rpos++ = *lpos;
        }
        lpos++;
    }

    parser_log_error("expected closing double quote");
    lib_free(result);
    *value = NULL;
    return false;
}

static bool get_int_arg(int *value, char **endptr)
{
    char *pos;
    long  result;
    int   base = 10;

    pos = lineptr;
    if (lineptr[0] == '0') {
        if (lineptr[1] == 'b' || lineptr[1] == 'B') {
            base = 2;
            pos  = lineptr + 2;
        } else if (lineptr[1] == 'x' || lineptr[1] == 'X') {
            base = 16;
            pos  = lineptr + 2;
        }
    } else if (lineptr[0] == '%') {
        base = 2;
        pos  = lineptr + 1;
    } else if (lineptr[0] == '$') {
        base = 16;
        pos  = lineptr + 1;
    }

    errno = 0;
    result = strtol(pos, endptr, base);
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
    if (major < 0) {
        parser_log_error("major version number cannot be less than 0");
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
    if (minor < 0) {
        parser_log_error("minor version number cannot be less than 0");
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
    int   version;
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

        case VJM_KW_DEVICE_VENDOR:
            if (!get_int_arg(&vendor, &endptr)) {
                return false;
            }
            if (vendor < 0 || vendor > 0xffff) {
                parser_log_error("illegal value %d for device vendor ID", vendor);
                return false;
            }
            joymap->dev_vendor = (uint16_t)vendor;
            break;

        case VJM_KW_DEVICE_PRODUCT:
            if (!get_int_arg(&product, &endptr)) {
                return false;
            }
            if (product < 0 || product > 0xffff) {
                parser_log_error("illegal value %d for device product ID", product);
                return false;
            }
            joymap->dev_product = (uint16_t)product;
            break;

        case VJM_KW_DEVICE_VERSION:
            if (!get_int_arg(&version, &endptr)) {
                return false;
            }
            if (version < 0 || version > 0xffff) {
                parser_log_error("illegal value %d for device version", version);
                return false;
            }
            joymap->dev_version = (uint16_t)version;
            break;

        case VJM_KW_DEVICE_NAME:
            if (!get_quoted_arg(&(joymap->dev_name), &endptr)) {
                return false;
            }
            msg_debug("got device name '%s'\n", joymap->dev_name);
            break;

        case VJM_KW_MAP:
            msg_debug("got keyword `map`: TODO\n");
            break;

        default:
            parser_log_warning("unexpected keyword '%s'", keywords[kw]);
            break;
    }
    return true;
}


static bool joymap_parse_line(joymap_t *joymap)
{
    keyword_id_t  kw;
    char         *endptr;

    lineptr = linebuf;

    skip_whitespace();
    if (*lineptr == '\0' || *lineptr == VJM_COMMENT) {
        return true;
    }
    msg_debug("parsing line %d: \"%s\"\n", linenum, lineptr);

    /* get keyword */
    kw = get_keyword(&endptr);
    if (kw == VJM_KW_INVALID) {
        *endptr = '\0';
        parser_log_error("unknown keyword '%s'", lineptr);
        return false;
    }
    printf("found keyword: %d: %s\n", (int)kw, keywords[kw]);
    lineptr = endptr;
    skip_whitespace();

    return handle_keyword(joymap, kw);
}


/** \brief  Load joymap from file
 *
 * \param[in]   path    path to joymap file
 *
 *
 * \return  new joymap object or \c NULL on error
 */
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


/** \brief  Dump joymap on stdout
 *
 * \param[in]   joymap  joymap
 */
void joymap_dump(joymap_t *joymap)
{
    printf("VJM version   : %d.%d\n", joymap->ver_major, joymap->ver_minor);
    printf("device vendor : %04x\n", (unsigned int)joymap->dev_vendor);
    printf("device product: %04x\n", (unsigned int)joymap->dev_product);
    printf("device version: %04x\n", (unsigned int)joymap->dev_version);
    if (joymap->dev_name != NULL) {
        printf("device name   : \"%s\"\n", joymap->dev_name);
    } else {
        printf("device name   : (none)\n");
    }
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
