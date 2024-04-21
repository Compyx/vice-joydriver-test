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

#include "joyapi.h"
#include "keyboard.h"
#include "lib.h"

#include "joymap.h"


#define VJM_COMMENT '#'

#define LINEBUF_INITIAL_SIZE    256


typedef enum {
    VJM_KW_INVALID = -1,

    VJM_KW_ACTION = 0,
    VJM_KW_AXIS,
    VJM_KW_BUTTON,
    VJM_KW_DEVICE_NAME,
    VJM_KW_DEVICE_PRODUCT,
    VJM_KW_DEVICE_VENDOR,
    VJM_KW_DEVICE_VERSION,
    VJM_KW_DOWN,
    VJM_KW_EAST,
    VJM_KW_FIRE1,
    VJM_KW_FIRE2,
    VJM_KW_FIRE3,
    VJM_KW_HAT,
    VJM_KW_KEY,
    VJM_KW_LEFT,
    VJM_KW_MAP,
    VJM_KW_NEGATIVE,
    VJM_KW_NONE,
    VJM_KW_NORTH,
    VJM_KW_NORTHEAST,
    VJM_KW_NORTHWEST,
    VJM_KW_PIN,
    VJM_KW_POSITIVE,
    VJM_KW_POT,
    VJM_KW_RIGHT,
    VJM_KW_SOUTH,
    VJM_KW_SOUTHEAST,
    VJM_KW_SOUTHWEST,
    VJM_KW_UP,
    VJM_KW_VJM_VERSION,
    VJM_KW_WEST,
} keyword_id_t;


static const char *keywords[] = {
    "action",
    "axis",
    "button",
    "device-name",
    "device-product",
    "device-vendor",
    "device-version",
    "down",
    "east",
    "fire1",
    "fire2",
    "fire3",
    "hat",
    "key",
    "left",
    "map",
    "negative",
    "none",
    "north",
    "northeast",
    "northwest",
    "pin",
    "positive",
    "pot",
    "right",
    "south",
    "southeast",
    "southwest",
    "up",
    "vjm-version",
    "west",
};


static char   *linebuf;
static size_t  linebuf_size;
static size_t  linebuf_len;
static int     linenum;
char          *lineptr;
const char    *current_path;


static bool kw_is_input_type(keyword_id_t kw)
{
    return (bool)(kw == VJM_KW_AXIS || kw == VJM_KW_BUTTON || kw == VJM_KW_HAT);
}

static bool kw_is_hat_direction(keyword_id_t kw)
{
    return (bool)(kw == VJM_KW_NORTH || kw == VJM_KW_NORTHEAST ||
                  kw == VJM_KW_EAST  || kw == VJM_KW_SOUTHEAST ||
                  kw == VJM_KW_SOUTH || kw == VJM_KW_SOUTHWEST ||
                  kw == VJM_KW_WEST  || kw == VJM_KW_NORTHWEST);
}

static bool kw_is_joystick_direction(keyword_id_t kw)
{
    return (bool)(kw == VJM_KW_UP   || kw == VJM_KW_DOWN ||
                  kw == VJM_KW_LEFT || kw == VJM_KW_RIGHT);
}

static bool kw_is_axis_direction(keyword_id_t kw)
{
    return (bool)(kw == VJM_KW_NEGATIVE || kw == VJM_KW_POSITIVE);
}

static bool kw_is_direction(keyword_id_t kw)
{
    return kw_is_hat_direction(kw) || kw_is_joystick_direction(kw) ||
        kw_is_axis_direction(kw);
}

/** \brief  Determine if pin number is valid
 *
 * Determine if \a pin is a directional pin or a fire[123] button pin.
 *
 * \param[in]   pin pin bit mask
 *
 * \return  \c true if \a pin is valid
 */
static bool pin_is_valid(int pin)
{
    return (bool)(pin == 1  || pin == 2  || pin == 4 || pin == 8 ||
                  pin == 16 || pin == 32 || pin == 64);
}

static bool matrix_row_is_valid(int row)
{
    return (bool)(row >= KBD_ROW_JOY_KEYPAD && row <= 9);
}

static bool matrix_column_is_valid(int column)
{
    return (bool)(column >= 0 && column < KBD_COLS);
}

static bool matrix_flags_is_valid(int flags)
{
    return (bool)(flags >= 0 && flags < ((KBD_MOD_SHIFTLOCK * 2) - 1));
}

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

    joymap->joydev        = NULL;
    joymap->path          = NULL;
    joymap->fp            = NULL;
    joymap->ver_major     = 0;
    joymap->ver_minor     = 0;
    joymap->dev_name      = NULL;
    joymap->dev_vendor    = 0x0000;
    joymap->dev_product   = 0x0000;
    joymap->dev_version   = 0x0000;

    return joymap;
}


void joymap_free(joymap_t *joymap)
{
    if (joymap != NULL) {
        if (joymap->fp != NULL) {
            fclose(joymap->fp);
        }
        lib_free(joymap->path);
        lib_free(joymap->dev_name);
        lib_free(joymap);
    }
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

    /* keyword  = '[a-z][a-z0-9[-]'+ */
    if (!islower((unsigned char)*pos)) {
        return false;
    }
    pos++;
    while (*pos != '\0' &&
            (islower((unsigned char)*pos) || isdigit((unsigned char)*pos) || *pos == '-')) {
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

/** \brief  Get string inside double quotes
 *
 * \param[out]  value   string value
 * \param[out]  endptr  pointer to character after closing quote
 *
 * \note    free result with \c lib_free()
 */
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
                    *endptr = lpos + 1;
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

/** \brief  Get integer argument from current position in line
 *
 * \param[out]  value   integer value
 * \param[out]  endptr  pointer to character after input
 *
 * \return  \c true on success
 *
 * \note    logs errors with \c parser_log_error()
 */
static bool get_int_arg(int *value, char **endptr)
{
    char *pos;
    long  result;
    int   base = 10;

    pos = lineptr;
    /* check prefixes */
    if (lineptr[0] == '0') {
        if (lineptr[1] == 'b' || lineptr[1] == 'B') {
            /* 0bNNNN -> binary */ 
            base = 2;
            pos  = lineptr + 2;
        } else if (lineptr[1] == 'x' || lineptr[1] == 'X') {
            /* 0xNNNN -> hex */
            base = 16;
            pos  = lineptr + 2;
        }
    } else if (lineptr[0] == '%') {
        /* %NNNN -> binary */
        base = 2;
        pos  = lineptr + 1;
    } else if (lineptr[0] == '$') {
        /* $NNNN -> hex */
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

static bool handle_pin_mapping(joymap_t *joymap)
{
    int            pin;
    keyword_id_t   input_type;
    keyword_id_t   input_direction = VJM_KW_NONE;
    char          *input_name = NULL;
    char          *endptr;
    joy_axis_t    *axis;
    joy_button_t  *button;
    joy_mapping_t *mapping;

    /* pin number */
    skip_whitespace();
    if (!get_int_arg(&pin, &endptr)) {
        /* TODO: check for up, down, left, right, fire[1-3] */
        parser_log_error("expected joystick pin number");
        return false;
    }
    if (!pin_is_valid(pin)) {
        parser_log_error("invalid pin number %d", pin);
        return false;
    }
    lineptr = endptr;

    /* input type */
    skip_whitespace();
    input_type = get_keyword(&endptr);
    if (!kw_is_input_type(input_type)) {
        parser_log_error("expected input type ('axis', 'button' or 'hat')");
        return false;
    }
    lineptr = endptr;

    /* input name */
    skip_whitespace();
    if (!get_quoted_arg(&input_name, &endptr)) {
        parser_log_error("expected input name");
        return false;
    }
    lineptr = endptr;
    msg_debug("line after input name: '%s'\n", lineptr);

    if (input_type != VJM_KW_BUTTON) {
        /* axes and hats require a direction argument */
        skip_whitespace();
        input_direction = get_keyword(&endptr);
        if (!kw_is_direction(input_direction)) {
            parser_log_error("expected direction argument for %s input",
                             keywords[input_type]);
            lib_free(input_name);
            return false;
        }
    }

    switch (input_type) {
        case VJM_KW_AXIS:
            if (!kw_is_axis_direction(input_direction)) {
                parser_log_error("invalid axis direction, got '%s', expected "
                                 "'negative' or 'positive'",
                                 keywords[input_direction]);
                lib_free(input_name);
                return false;
            }
            axis = joy_axis_from_name(joymap->joydev, input_name);
            if (axis == NULL) {
                parser_log_error("failed to find axis '%s'", input_name);
                lib_free(input_name);
                return false;
            }

            /* select negative or positive mapping */
            if (input_direction == VJM_KW_NEGATIVE) {
                mapping = &(axis->negative_mapping);
            } else {
                mapping = &(axis->positive_mapping);
            }
            mapping->action     = JOY_ACTION_JOYSTICK;
            mapping->target.pin = pin;
            break;

        case VJM_KW_BUTTON:
            button = joy_button_from_name(joymap->joydev, input_name);
            if (button == NULL) {
                parser_log_error("failed to find button '%s'", input_name);
                lib_free(input_name);
                return false;
            }
            button->mapping.action     = JOY_ACTION_JOYSTICK;
            button->mapping.target.pin = pin;
            break;

        case VJM_KW_HAT:
            parser_log_warning("TODO: mapping pin to hat\n");
            break;

        default:
            parser_log_error("unhandled input type %d!\n", (int)input_type);
            break;
    }

    printf("got pin number %d, input type %s, input name %s, input direction %s\n",
           pin, keywords[input_type], input_name, keywords[input_direction]);
    lib_free(input_name);
    return true;
}

static bool handle_key_mapping(joymap_t *joymap)
{
    int           column;
    int           row;
    int           flags;
    char         *endptr;
    char         *input_name = NULL;
    keyword_id_t  input_type;
    keyword_id_t  input_direction;

    /* row */
    skip_whitespace();
    if (!get_int_arg(&row, &endptr)) {
        parser_log_error("expected keyboard matrix row number");
        return false;
    }
    if (!matrix_row_is_valid(row)) {
        parser_log_error("keyboard matrix row %d out of range", row);
        return false;
    }
    lineptr = endptr;

    /* column */
    skip_whitespace();
    if (!get_int_arg(&column, &endptr)) {
        parser_log_error("expected keyboard matrix column number");
        return false;
    }
    if (!matrix_column_is_valid(column)) {
        parser_log_error("keyboard matrix column %d out of range", column);
        return false;
    }
    lineptr = endptr;

    /* flags */
    skip_whitespace();
    if (!get_int_arg(&flags, &endptr)) {
        /* TODO: support keywords for flags */
        parser_log_error("expected keyboard modifier flags");
        return false;
    }
    if (!matrix_flags_is_valid(flags)) {
        parser_log_error("invalid keyboard modifier flags: %d (%04x)",
                         flags, (unsigned int)flags);
        return false;
    }
    lineptr = endptr;

    /* input type */
    skip_whitespace();
    input_type = get_keyword(&endptr);
    if (!kw_is_input_type(input_type)) {
        parser_log_error("expected input type ('axis', 'button' or 'hat')");
        return false;
    }
    lineptr = endptr;

    /* input name */
    skip_whitespace();
    if (!get_quoted_arg(&input_name, &endptr)) {
        parser_log_error("expected input name");
        return false;
    }
    lineptr = endptr;

    if (input_type != VJM_KW_BUTTON) {
        /* axes and hats require a direction argument */
        skip_whitespace();
        input_direction = get_keyword(&endptr);
        if (!kw_is_direction(input_direction)) {
            parser_log_error("expected direction argument for %s input",
                             keywords[input_type]);
            lib_free(input_name);
            return false;
        }
    }




    printf("got key column %d, row %d, flags %04x, input type %s, input name %s\n",
           column, row, (unsigned int)flags, keywords[input_type], input_name); 
    lib_free(input_name);
    return true;
}

static bool handle_mapping(joymap_t *joymap)
{
    char *endptr;
    bool  result = true;

    skip_whitespace();

    switch (get_keyword(&endptr)) {
        case VJM_KW_PIN:
            /* "pin <pin#> <input-type> <input-name> [<input-args>]" */
            lineptr = endptr;
            result = handle_pin_mapping(joymap);
            break;

        case VJM_KW_POT:
            parser_log_warning("TODO: handle 'pot'");
            break;

        case VJM_KW_KEY:
            /* "key <column> <row> <flags> <input-name>" */
            lineptr = endptr;
            result = handle_key_mapping(joymap);
            break;

        case VJM_KW_ACTION:
            parser_log_warning("TODO: handle 'action'");
            break;

        default:
            parser_log_error("expected either 'pin', 'pot', 'key' or 'action'");
            result = false;
            break;
    }

    return result;
}

static bool handle_keyword(joymap_t *joymap, keyword_id_t kw)
{
    int   vendor;
    int   product;
    int   version;
    char *endptr;
    bool  result = true;

    if (*lineptr == '\0') {
        parser_log_error("missing data after keyword '%s'", keywords[kw]);
        return false;
    }

    switch (kw) {
        case VJM_KW_VJM_VERSION:
            if (!get_vjm_version(joymap)) {
                result = false;
            } else {
                msg_debug("VJM version: %d.%d\n", joymap->ver_major, joymap->ver_minor);
            }
            break;

        case VJM_KW_DEVICE_VENDOR:
            if (!get_int_arg(&vendor, &endptr)) {
                result =  false;
            } else if (vendor < 0 || vendor > 0xffff) {
                parser_log_error("illegal value %d for device vendor ID", vendor);
                result = false;
            } else {
                joymap->dev_vendor = (uint16_t)vendor;
            }
            break;

        case VJM_KW_DEVICE_PRODUCT:
            if (!get_int_arg(&product, &endptr)) {
                result = false;
            } else if (product < 0 || product > 0xffff) {
                parser_log_error("illegal value %d for device product ID", product);
                result = false;
            } else {
                joymap->dev_product = (uint16_t)product;
            }
            break;

        case VJM_KW_DEVICE_VERSION:
            if (!get_int_arg(&version, &endptr)) {
                result = false;
            } else if (version < 0 || version > 0xffff) {
                parser_log_error("illegal value %d for device version", version);
                result = false;
            } else {
                joymap->dev_version = (uint16_t)version;
            }
            break;

        case VJM_KW_DEVICE_NAME:
            if (!get_quoted_arg(&(joymap->dev_name), &endptr)) {
                result = false;
            } else {
                msg_debug("got device name '%s'\n", joymap->dev_name);
            }
            break;

        case VJM_KW_MAP:
            if (!handle_mapping(joymap)) {
                result = false;
            }
            break;

        default:
            parser_log_error("unexpected keyword '%s'", keywords[kw]);
            result = false;
            break;
    }
    return result;
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
 * \param[in]   joydev  joystick device
 * \param[in]   path    path to joymap file
 *
 *
 * \return  new joymap object or \c NULL on error
 */
joymap_t *joymap_load(joy_device_t *joydev, const char *path)
{
    joymap_t *joymap = NULL;

    if (joydev == NULL) {
        msg_error("`joydev` argument cannot be NULL");
        return NULL;
    }

    current_path = path;

    msg_debug("loading joymap file '%s'\n", path);
    joymap = joymap_open(path);
    if (joymap == NULL) {
        return NULL;
    }
    joymap->joydev = joydev;

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
