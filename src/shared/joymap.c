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

/** \brief  Parser state object */
typedef struct pstate_s {
    const char *current_path;   /**< current path to vjm file */
    char       *buffer;         /**< buffer for reading lines from file */
    size_t      bufsize;        /**< number of bytes allocated for \c buffer */
    size_t      buflen;         /**< string length of \c buffer */
    int         linenum;        /**< line number in vjm file */
    char       *curpos;         /**< current position in buffer */
    char       *prevpos;        /**< previous position in buffer for error
                                     reporting */
} pstate_t;


/* forward declarations */
static void parser_log_warning(const char *fmt, ...);
static void parser_log_error  (const char *fmt, ...);

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


/** \brief  Parser state
 *
 * Parser state is kept in a struct to allow for using simple identifiers
 * without causing conflicts with other identifiers, and to make it simple to
 * pass around a reference to the state to external modules, might that be
 * required later.
 */
static pstate_t pstate;


/** \brief  Initialize parser state
 *
 * Initialize parser for use, allocating an initial buffer for reading lines
 * from a file.
 */
static void pstate_init(void)
{
    pstate.current_path = NULL;
    pstate.bufsize   = LINEBUF_INITIAL_SIZE;
    pstate.buflen    = 0;
    pstate.buffer    = lib_malloc(pstate.bufsize);
    pstate.buffer[0] = '\0';
    pstate.curpos    = pstate.buffer;
    pstate.prevpos   = pstate.buffer;
    pstate.linenum   = -1;
};

/** \brief  Clean up resources used by the parser state */
static void pstate_free(void)
{
    lib_free(pstate.buffer);
}

/** \brief  Skip whitespace in the buffer to the next token */
static void pstate_skip_whitespace(void)
{
    while (*(pstate.curpos) != '\0' && isspace((unsigned char)(*pstate.curpos))) {
        pstate.curpos++;
    }
}

/** \brief  Strip trailing whitespace from line buffer */
static void pstate_rtrim(void)
{
    char *s = pstate.buffer + pstate.buflen - 1;

    while (s >= pstate.buffer && isspace((unsigned char)*s)) {
        *s-- = '\0';
    }
}

/** \brief  Set new position in buffer and skip whitespace until next token
 *
 * \param[in]   newpos  new position in the parser's line buffer
 */
static void pstate_update(char *newpos)
{
    if (newpos == NULL) {
        parser_log_error("newpos is NULL!");
        return;
    }
    pstate.prevpos = pstate.curpos;
    pstate.curpos  = newpos;
    pstate_skip_whitespace();
}

#if 0
static bool pstate_is_number(void)
{
    char *s = pstate.curpos;

    if (s[0] == '-' && isdigit((unsigned char)s[1])) {
        return true;
    } else if (s[0] == '0') {
        if (s[1] == '\0' || isspace((unsigned char)s[1])) {
            return true;
        }
        if ((s[1] == 'b' || s[1] == 'B') && (s[2] == '0' || s[2] == '1')) {
            return true;
        }
        if ((s[1] == 'x' || s[1] == 'X') && isxdigit((unsigned char)s[2])) {
            return true;
        }
    } else if (s[0] == '%' && (s[1] == '0' || s[1] == '1')) {
        return true;
    } else if (s[0] == '$' && isxdigit((unsigned char)s[1])) {
        return true;
    }
    return false;
}
#endif

/** \brief  Get keyword name by ID
 *
 * \param[in]   kw  keyword ID
 *
 * \return  keyword name (or "&gt;invalid&lt;" when \a kw is invalid)
 */
static const char *kw_name(keyword_id_t kw)
{
    if (kw >= 0 || kw < (keyword_id_t)(sizeof keywords / sizeof keywords[0])) {
        return keywords[kw];
    }
    return "<invalid>";
}

/** \brief  Determine if keyword is an input type
 *
 * \return  \c true if input type
 */
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


static void parser_log_helper(const char *prefix, const char *fmt, va_list args)
{
    char msg[1024];

    vsnprintf(msg, sizeof msg, fmt, args);
    if (prefix != NULL) {
        fprintf(stderr, "%s: ", prefix);
    }
    fprintf(stderr,
            "%s:%d:%d: %s\n",
            lib_basename(pstate.current_path),
            pstate.linenum,
            (int)(pstate.curpos - pstate.buffer) + 1,
            msg);
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

    pstate.linenum   = 1;
    pstate.buffer[0] = '\0';
    pstate.curpos    = pstate.buffer;
    pstate.prevpos   = pstate.buffer;

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
    pstate.buflen    = 0;
    pstate.buffer[0] = '\0';
    pstate.curpos    = pstate.buffer;
    pstate.prevpos   = pstate.buffer;

    while (true) {
        int ch;

        if (pstate.buflen - 1u == pstate.bufsize) {
            pstate.bufsize *= 2u;
            pstate.buffer = lib_realloc(pstate.buffer, pstate.bufsize);
        }

        ch = fgetc(joymap->fp);
        if (ch == EOF) {
            pstate.buffer[pstate.buflen] = '\0';
            pstate_rtrim();
            return false;
        } else if (ch == '\n') {
            break;
        }
        pstate.buffer[pstate.buflen++] = (char)ch;
    }

    pstate.buffer[pstate.buflen] = '\0';
    pstate_rtrim();
    pstate.linenum++;
    return true;
}

static keyword_id_t get_keyword(void)
{
    keyword_id_t  id  = VJM_KW_INVALID;
    char         *pos = pstate.curpos;

    /* keyword  = '[a-z][a-z0-9[-]'+ */
    if (!islower((unsigned char)*pos)) {
        return VJM_KW_INVALID;
    }
    pos++;
    while (*pos != '\0' &&
            (islower((unsigned char)*pos) || isdigit((unsigned char)*pos) || *pos == '-')) {
        pos++;
    }
    if (pos > pstate.curpos) {
        size_t i;

        for (i = 0; i < ARRAY_LEN(keywords); i++) {
            if (strncmp(keywords[i], pstate.curpos, (size_t)(pos - pstate.curpos)) == 0) {
                id = (keyword_id_t)i;
                pstate_update(pos);
                break;
            }
        }
    }
    return id;
}

/** \brief  Get string inside double quotes
 *
 * \param[out]  value   string value
 *
 * \note    free result with \c lib_free()
 */
static bool get_quoted_arg(char **value)
{
    char *result;
    char *rpos;
    char *lpos;
    bool  escaped;

    if (*pstate.curpos != '"') {
        parser_log_error("expected opening double quote");
        *value = NULL;
        return false;
    }

    result  = lib_malloc(pstate.buflen - (size_t)(pstate.curpos - pstate.buffer) + 1u);
    escaped = false;
    lpos    = pstate.curpos + 1;
    rpos    = result;
    while (*lpos != '\0') {
        if (*lpos == '"') {
            if (!escaped) {
                /* end of argument */
                *rpos  = '\0';
                *value = result;
                pstate_update(lpos + 1);
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
 *
 * \return  \c true on success
 *
 * \note    logs errors with \c parser_log_error()
 */
static bool get_int_arg(int *value)
{
    char *s;
    char *endptr;
    long  result;
    int   base = 10;

    s = pstate.curpos;
    /* check prefixes */
    if (s[0] == '0') {
        if (s[1] == 'b' || s[1] == 'B') {
            /* 0bNNNN -> binary */ 
            base = 2;
            s += 2;
        } else if (s[1] == 'x' || s[1] == 'X') {
            /* 0xNNNN -> hex */
            base = 16;
            s += 2;
        }
    } else if (s[0] == '%') {
        /* %NNNN -> binary */
        base = 2;
        s++;
    } else if (s[0] == '$') {
        /* $NNNN -> hex */
        base = 16;
        s++;
    }

    errno = 0;
    result = strtol(s, &endptr, base);
    if (errno != 0) {
        parser_log_error("failed to convert '%s' to long", pstate.curpos);
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
    pstate_update(endptr);
    return true;
}


static bool get_vjm_version(joymap_t *joymap)
{
    int major = 0;
    int minor = 0;

    if (!get_int_arg(&major)) {
        parser_log_error("expected major version number");
        return false;
    }
    if (major < 0) {
        parser_log_error("major version number cannot be less than 0");
        return false;
    }

    /* check for period and skip if present */
    if (*pstate.curpos != '.') {
        parser_log_error("expected dot after major version number");
        return false;
    }
    pstate.curpos++;

    if (!get_int_arg(&minor)) {
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


/** \brief  Get mapping for axis
 *
 * Parse current line for axis name and look up axis mapping in \a joymap.
 * Requires parser to be beyond the input type ('axis').
 *
 * \param[in]   joymap  joymap
 *
 * \return  mapping or \c NULL on error
 */
static joy_mapping_t *get_axis_mapping(joymap_t *joymap)
{
    joy_mapping_t *mapping;
    joy_axis_t    *axis;
    char          *name;
    keyword_id_t   direction;

    /* input name */
    if (!get_quoted_arg(&name)) {
        parser_log_error("expected axis name");
        return NULL;
    }
    /* input direction */
    direction = get_keyword();
    if (!kw_is_axis_direction(direction)) {
        parser_log_error("expected axis direction argument ('negative' or"
                         " 'positive'), got '%s'",
                         pstate.curpos);
        lib_free(name);
        return NULL;
    }

    axis = joy_axis_from_name(joymap->joydev, name);
    if (axis == NULL) {
        parser_log_error("invalid axis name: '%s'", name);
        lib_free(name);
        return NULL;
    }
    if (direction == VJM_KW_NEGATIVE) {
        mapping = &(axis->negative_mapping);
    } else {
        mapping = &(axis->positive_mapping);
    }

    msg_debug("name = %s, direction = %s\n", name, kw_name(direction));
    lib_free(name);

    return mapping;
}

/** \brief  Get mapping for button
 *
 * Parse current line for button name and look up button mapping in \a joymap.
 * Requires parser to be beyond the input type ('button').
 *
 * \param[in]   joymap  joymap
 *
 * \return  mapping or \c NULL on error
 */
static joy_mapping_t *get_button_mapping(joymap_t *joymap)
{
    joy_button_t *button;
    char         *name;

    if (!get_quoted_arg(&name)) {
        parser_log_error("expected button name");
        return NULL;
    }

    button = joy_button_from_name(joymap->joydev, name);
    if (button == NULL) {
        parser_log_error("invalid button name: '%s'", name);
        lib_free(name);
        return NULL;
    }
    
    msg_debug("name = %s\n", name);
    lib_free(name);

    return &(button->mapping);
}

/** \brief  Get mapping for hat
 *
 * Parse current line for hat name and look up hat mapping in \a joymap.
 * Requires parser to be beyond the input type ('hat').
 *
 * \param[in]   joymap  joymap
 *
 * \return  mapping or \c NULL on error
 */
static joy_mapping_t *get_hat_mapping(joymap_t *joymap)
{
    joy_hat_t    *hat;
    char         *name;
#if 0
    keyword_id_t  direction;
#endif

    if (!get_quoted_arg(&name)) {
        parser_log_error("exected hat name");
        return NULL;
    }

    hat = joy_hat_from_name(joymap->joydev, name);
    if (hat == NULL) {
        parser_log_error("invalid hat name: '%s'", name);
        lib_free(name);
        return NULL;
    }

    msg_debug("name = %s\n", name);
    lib_free(name);

    return &(hat->mapping);
}


static bool handle_pin_mapping(joymap_t *joymap)
{
    int            pin;
    keyword_id_t   input_type;
    joy_mapping_t *mapping;

    /* pin number */
    if (!get_int_arg(&pin)) {
        /* TODO: check for up, down, left, right, fire[1-3] */
        parser_log_error("expected joystick pin number");
        return false;
    }
    if (!pin_is_valid(pin)) {
        parser_log_error("invalid pin number %d", pin);
        return false;
    }

    /* input type */
    input_type = get_keyword();
    if (!kw_is_input_type(input_type)) {
        parser_log_error("expected input type ('axis', 'button' or 'hat')");
        return false;
    }

    switch (input_type) {
        case VJM_KW_AXIS:
            mapping = get_axis_mapping(joymap);
            break;

        case VJM_KW_BUTTON:
            mapping = get_button_mapping(joymap);
            break;

        case VJM_KW_HAT:
            mapping = get_hat_mapping(joymap);
            break;

        default:
            parser_log_error("unhandled input type %d!\n", (int)input_type);
            mapping = NULL;
            break;
    }

    if (mapping != NULL) {
        msg_debug("mapping to pin %d\n", pin);
        mapping->action = JOY_ACTION_JOYSTICK;
        mapping->target.pin = pin;
        return true;
    } else {
        return false;
    }
}


static bool handle_key_mapping(joymap_t *joymap)
{
    int           column;
    int           row;
    int           flags;
    char         *input_name = NULL;
    keyword_id_t  input_type;
    keyword_id_t  input_direction;
    joy_axis_t   *axis;
    joy_mapping_t *mapping;

    /* row */
    if (!get_int_arg(&row)) {
        parser_log_error("expected keyboard matrix row number");
        return false;
    }
    if (!matrix_row_is_valid(row)) {
        parser_log_error("keyboard matrix row %d out of range", row);
        return false;
    }

    /* column */
    if (!get_int_arg(&column)) {
        parser_log_error("expected keyboard matrix column number");
        return false;
    }
    if (!matrix_column_is_valid(column)) {
        parser_log_error("keyboard matrix column %d out of range", column);
        return false;
    }

    /* flags */
    if (!get_int_arg(&flags)) {
        /* TODO: support keywords for flags */
        parser_log_error("expected keyboard modifier flags");
        return false;
    }
    if (!matrix_flags_is_valid(flags)) {
        parser_log_error("invalid keyboard modifier flags: %d (%04x)",
                         flags, (unsigned int)flags);
        return false;
    }

    /* input type */
    input_type = get_keyword();
    if (!kw_is_input_type(input_type)) {
        parser_log_error("expected input type ('axis', 'button' or 'hat')");
        return false;
    }

    /* input name */
    if (!get_quoted_arg(&input_name)) {
        parser_log_error("expected input name");
        return false;
    }

    if (input_type != VJM_KW_BUTTON) {
        /* axes and hats require a direction argument */
        input_direction = get_keyword();
        if (!kw_is_direction(input_direction)) {
            parser_log_error("expected direction argument for %s input",
                             kw_name(input_type));
            lib_free(input_name);
            return false;
        }
    }

    switch (input_type) {
        case VJM_KW_AXIS:
            if (!kw_is_axis_direction(input_direction)) {
                parser_log_error("invalid axis direction, got '%s', expected "
                                 "'negative' or 'positive'",
                                 kw_name(input_direction));
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
            mapping->action            = JOY_ACTION_KEYBOARD;
            mapping->target.key.row    = row;
            mapping->target.key.column = column;
            mapping->target.key.flags  = (unsigned int)flags; 
            break;

        default:
            break;
    }


    printf("got key column %d, row %d, flags %04x, input type %s, input name %s\n",
           column, row, (unsigned int)flags, kw_name(input_type), input_name); 
    lib_free(input_name);
    return true;
}

static bool handle_mapping(joymap_t *joymap)
{
    bool result = true;

    switch (get_keyword()) {
        case VJM_KW_PIN:
            /* "pin <pin#> <input-type> <input-name> [<input-args>]" */
            result = handle_pin_mapping(joymap);
            break;

        case VJM_KW_POT:
            parser_log_warning("TODO: handle 'pot'");
            break;

        case VJM_KW_KEY:
            /* "key <column> <row> <flags> <input-name>" */
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
    bool  result = true;

    if (*pstate.curpos == '\0') {
        parser_log_error("missing data after keyword '%s'", kw_name(kw));
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
            if (!get_int_arg(&vendor)) {
                result =  false;
            } else if (vendor < 0 || vendor > 0xffff) {
                parser_log_error("illegal value %d for device vendor ID", vendor);
                result = false;
            } else {
                joymap->dev_vendor = (uint16_t)vendor;
            }
            break;

        case VJM_KW_DEVICE_PRODUCT:
            if (!get_int_arg(&product)) {
                result = false;
            } else if (product < 0 || product > 0xffff) {
                parser_log_error("illegal value %d for device product ID", product);
                result = false;
            } else {
                joymap->dev_product = (uint16_t)product;
            }
            break;

        case VJM_KW_DEVICE_VERSION:
            if (!get_int_arg(&version)) {
                result = false;
            } else if (version < 0 || version > 0xffff) {
                parser_log_error("illegal value %d for device version", version);
                result = false;
            } else {
                joymap->dev_version = (uint16_t)version;
            }
            break;

        case VJM_KW_DEVICE_NAME:
            if (!get_quoted_arg(&(joymap->dev_name))) {
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
            parser_log_error("unexpected keyword '%s'", kw_name(kw));
            result = false;
            break;
    }
    return result;
}


static bool joymap_parse_line(joymap_t *joymap)
{
    keyword_id_t  kw;

    pstate.curpos = pstate.buffer;
    pstate_skip_whitespace();

    if (*pstate.curpos == '\0' || *pstate.curpos == VJM_COMMENT) {
        return true;
    }
    msg_debug("parsing line %d: \"%s\"\n", pstate.linenum, pstate.curpos);

    /* get keyword */
    kw = get_keyword();
    if (kw == VJM_KW_INVALID) {
        parser_log_error("unknown keyword: %s", pstate.curpos);
        return false;
    }
    printf("found keyword: %d: %s\n", (int)kw, kw_name(kw));

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

    pstate.current_path = path;

    msg_debug("loading joymap file '%s'\n", path);
    joymap = joymap_open(path);
    if (joymap == NULL) {
        return NULL;
    }
    joymap->joydev = joydev;

    errno          = 0;
    pstate.linenum = 0;
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
    pstate_init();
}


void joymap_module_shutdown(void)
{
    pstate_free();
}
