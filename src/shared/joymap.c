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
#include "uiactions.h"

#include "joymap.h"


#define bstr(B) ((B) ? "true" : "false")

#define VJM_COMMENT '#'

#define LINEBUF_INITIAL_SIZE    256

/** \brief  VJM keyword IDs
 *
 * \note    Keep in the same order as the \c keywords array!
 */
typedef enum {
    VJM_KW_INVALID = -1,        /**< error code */

    VJM_KW_ACTION = 0,          /**< action */
    VJM_KW_AXIS,                /**< axis */
    VJM_KW_BUTTON,              /**< button */
    VJM_KW_CALIBRATE,           /**< calibrate */
    VJM_KW_DEADZONE,            /**< deadzone */
    VJM_KW_DEVICE_NAME,         /**< device-name */
    VJM_KW_DEVICE_PRODUCT,      /**< device-product */
    VJM_KW_DEVICE_VENDOR,       /**< device-vendor */
    VJM_KW_DEVICE_VERSION,      /**< device-version */
    VJM_KW_DOWN,                /**< down */
    VJM_KW_FIRE1,               /**< fire1 */
    VJM_KW_FIRE2,               /**< fire2 */
    VJM_KW_FIRE3,               /**< fire3 */
    VJM_KW_FUZZ,                /**< fuzz */
    VJM_KW_HAT,                 /**< hat */
    VJM_KW_INVERTED,            /**< inverted */
    VJM_KW_KEY,                 /**< key */
    VJM_KW_LEFT,                /**< left */
    VJM_KW_MAP,                 /**< map */
    VJM_KW_NEGATIVE,            /**< negative */
    VJM_KW_NONE,                /**< none */
    VJM_KW_PIN,                 /**< pin */
    VJM_KW_POSITIVE,            /**< positive */
    VJM_KW_POT,                 /**< pot */
    VJM_KW_RIGHT,               /**< right */
    VJM_KW_THRESHOLD,           /**< threshold */
    VJM_KW_UP,                  /**< up */
    VJM_KW_VJM_VERSION,         /**< vjm-version */
} keyword_id_t;

/** \brief  Parser state object */
typedef struct pstate_s {
    FILE       *fp;         /**< file pointer during parsing */
    const char *path;       /**< current path to vjm file */
    char       *buffer;     /**< buffer for reading lines from file */
    size_t      bufsize;    /**< number of bytes allocated for \c buffer */
    size_t      buflen;     /**< string length of \c buffer */
    int         linenum;    /**< line number in vjm file */
    char       *curpos;     /**< current position in buffer */
    char       *prevpos;    /**< previous position in buffer for error logging */
} pstate_t;


/* forward declarations */
static void parser_log_warning(const char *fmt, ...);
static void parser_log_error  (const char *fmt, ...);

/** \brief  VJM keywords
 *
 * List of keywords recognized by the parser.
 *
 * \note    Keep these alphabetically sorted!
 */
static const char *keywords[] = {
    "action",
    "axis",
    "button",
    "calibrate",
    "deadzone",
    "device-name",
    "device-product",
    "device-vendor",
    "device-version",
    "down",
    "fire1",
    "fire2",
    "fire3",
    "fuzz",
    "hat",
    "inverted",
    "key",
    "left",
    "map",
    "negative",
    "none",
    "pin",
    "positive",
    "pot",
    "right",
    "threshold",
    "up",
    "vjm-version"
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
    pstate.fp        = NULL;
    pstate.path      = NULL;
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
    if (pstate.fp != NULL) {
        fclose(pstate.fp);
    }
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

    if ((s[0] == '-' || s[0] == '+') && isdigit((unsigned char)s[1])) {
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

#if 0
/** \brief  Determine if keyword is an input type
 *
 * Check keyword for "axis", "button" or "hat".
 *
 * \param[in]   kw  keyword ID
 *
 * \return  \c true if input type
 */
static bool kw_is_input_type(keyword_id_t kw)
{
    return (bool)(kw == VJM_KW_AXIS || kw == VJM_KW_BUTTON || kw == VJM_KW_HAT);
}
#endif

/** \brief  Determine if keyword is a joystick direction
 *
 * Check keyword for "up", "down", "left" or "right".
 *
 * \param[in]   kw  keyword ID
 *
 * \return  \c true if valid joystick direction
 */
static bool kw_is_joystick_direction(keyword_id_t kw)
{
    return (bool)(kw == VJM_KW_UP   || kw == VJM_KW_DOWN ||
                  kw == VJM_KW_LEFT || kw == VJM_KW_RIGHT);
}

/** \brief  Determine if keyword is an axis direction
 *
 * Check keyword for "negative" or "positive".
 *
 * \param[in]   kw  keyword ID
 *
 * \return  \c true if valid axis direction
 */
static bool kw_is_axis_direction(keyword_id_t kw)
{
    return (bool)(kw == VJM_KW_NEGATIVE || kw == VJM_KW_POSITIVE);
}

/** \brief  Determine if pin number is valid
 *
 * Determine if \a pin is a directional pin, a fire[123] button, or a SNES pad
 * button (A, B, X, Y, L, R, Select, Start).
 *
 * \param[in]   pin pin bit
 *
 * \return  \c true if \a pin is valid
 */
static bool pin_is_valid(int pin)
{
    return (bool)(pin == 1   || pin == 2   || pin == 4    || pin == 8 ||
                  pin == 16  || pin == 32  || pin == 64   || pin == 128 ||
                  pin == 256 || pin == 512 || pin == 1024 || pin == 2048);
}

/** \brief  Determine if keyboard matrix row is valid
 *
 * \param[in]   row keyboard matrix row
 *
 * \return  \c true if valid
 */
static bool matrix_row_is_valid(int row)
{
    return (bool)(row >= KBD_ROW_JOY_KEYPAD && row <= 9);
}

/** \brief  Determine if keyboard matrix column is valid
 *
 * \param[in]   column  keyboard matrix column
 *
 * \return  \c true if valid
 */
static bool matrix_column_is_valid(int column)
{
    return (bool)(column >= 0 && column < KBD_COLS);
}

/** \brief  Determine if key press flags value is valid
 *
 * \param[in]   flags   key press flags
 *
 * \return  \c true if valid
 */
static bool matrix_flags_is_valid(int flags)
{
    return (bool)(flags >= 0 && flags < ((KBD_MOD_SHIFTLOCK * 2) - 1));
}

/** \brief  Helper for logging parser messages
 *
 * \param[in]   prefix  prefix for messages (can be \c NULL)
 * \param[in]   fmt     format strng
 * \param[in]   args    arguments
 */
static void parser_log_helper(const char *prefix, const char *fmt, va_list args)
{
    char msg[1024];

    vsnprintf(msg, sizeof msg, fmt, args);
    if (prefix != NULL) {
        fprintf(stderr, "%s: ", prefix);
    }
    fprintf(stderr,
            "%s:%d:%d: %s\n",
            lib_basename(pstate.path),
            pstate.linenum,
            (int)(pstate.curpos - pstate.buffer) + 1,
            msg);
}

/** \brief  Log parser error message on stderr
 *
 * \param[in]   fmt format string
 * \param[in]   ... arguments for \a fmt
 */
static void parser_log_error(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    parser_log_helper("error", fmt, args);
    va_end(args);
}

/** \brief  Log parser warning message on stderr
 *
 * \param[in]   fmt format string
 * \param[in]   ... arguments for \a fmt
 */
static void parser_log_warning(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    parser_log_helper("warning", fmt, args);
    va_end(args);
}


/** \brief  Allocate and intialize new joymap object
 *
 * \return  new joymap with all fields initialized to \c 0 / \c NULL
 */
static joymap_t *joymap_new(void)
{
    joymap_t *joymap = lib_malloc(sizeof *joymap);

    joymap->joydev        = NULL;
    joymap->path          = NULL;
    joymap->ver_major     = 0;
    joymap->ver_minor     = 0;
    joymap->dev_name      = NULL;
    joymap->dev_vendor    = 0x0000;
    joymap->dev_product   = 0x0000;
    joymap->dev_version   = 0x0000;

    return joymap;
}


/** \brief  Free joymap and its members
 *
 * \param[in]   joymap  joymap
 */
void joymap_free(joymap_t *joymap)
{
    if (joymap != NULL) {
        lib_free(joymap->path);
        lib_free(joymap->dev_name);
        lib_free(joymap);
    }
    if (pstate.fp != NULL) {
        fclose(pstate.fp);
        pstate.fp = NULL;
    }
}


/** \brief  Open file and create new joymap object for it
 *
 * \param[in]   path    path to VJM file
 *
 * \return  new joymap or \c NULL on failure
 */
static joymap_t *joymap_open(const char *path)
{
    joymap_t *joymap;

    pstate.linenum   = 1;
    pstate.buffer[0] = '\0';
    pstate.curpos    = pstate.buffer;
    pstate.prevpos   = pstate.buffer;
    pstate.fp        = fopen(path, "r");

    if (pstate.fp == NULL) {
        msg_error("failed to open vjm file for reading: %s\n", strerror(errno));
        return NULL;
    }

    joymap = joymap_new();
    joymap->path = lib_strdup(path);
    return joymap;
}

/** \brief  Read a line from the joymap
 *
 * \return  \c true on success, \c false on EOF (can indicate failure)
 */
static bool joymap_read_line(void)
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

        ch = fgetc(pstate.fp);
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

/** \brief  Parse current position in line for keyword
 *
 * \return  keyword ID or \c VJM_KW_INVALID (-1) when no keyword was found
 */ 
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

/** \brief  Get action ID from current position in line buffer
 *
 * Get action name (optionally quoted with double quotes) and look up its ID.
 * If \a name is not \c NULL the action name parsed will be stored in \a name,
 * to be freed by the caller with \c lib_free(). This includes action names
 * that match the pattern for action names but for which an ID wasn't found.
 * Any errors during parsing or when looking up an ID will be reported by this
 * function.
 *
 * \param[out]  name    action name found, including invalid ones (can be \c NULL)
 *
 * \return  action ID (or \c ACTION_INVALID) when not found
 *
 * \note    free \c name with \c lib_free() if not set to \c NULL
 */
static int get_ui_action(char **name)
{
    char   *s;
    char   *action;
    size_t  len;
    int     id;
    bool    quoted = false;

    len = 0;
    s   = pstate.curpos;
    if (*s == '"') {
        quoted = true;
        s++;
    }

    //    printf("action name quoted = %s\n", quoted ? "TRUE" : "FALSE");
    while (IS_ACTION_NAME_CHAR(*s)) {
        s++;
    }
    if (quoted && *s != '"') {
        /* missing closing quote */
        parser_log_error("missing closing quote in UI action name");
        goto exit_err;
    }

    /* copy action name */
    if (quoted) {
        /* -1 to account for opening quote */
        len = (size_t)(s - pstate.curpos - 1);
    } else {
        len = (size_t)(s - pstate.curpos);
    }
    if (len == 0) {
        parser_log_error("missing action name");
        goto exit_err;
    }

    action = lib_malloc(len + 1u);
    memcpy(action, pstate.curpos + (quoted ? 1 : 0), len);
    action[len] = '\0';

    id = ui_action_get_id(action);
    if (id < ACTION_NONE) {
        parser_log_error("invalid action name '%s'", action);
    }

    if (name != NULL) {
        *name = action;
    } else {
        lib_free(action);
    }

    if (quoted) {
        pstate_update(s + 1);
    } else {
        pstate_update(s);
    }

    return id;


exit_err:
    if (name != NULL) {
        *name = NULL;
    }
    return ACTION_INVALID;
}

/** \brief  Get VJM version number
 *
 * Parse current line for VJM version (major.minor), called when encountering
 * the "vjm-version "keyword.
 *
 * \param[in]   joymap  joymap
 *
 * \return  \c true on success
 */
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


/** \brief  Get axis from current line
 *
 * Parse current line for axis name and direction.
 *
 * \param[in]   joymap      joymap
 * \param[out]  direction   axis direction
 *
 * \return  axis or \c NULL on failure
 */
static joy_axis_t *get_axis_and_direction(joymap_t     *joymap,
                                          keyword_id_t *direction)
{
    joy_axis_t *axis;
    char       *name;

    /* axis name */
    if (!get_quoted_arg(&name)) {
        parser_log_error("expected axis name");
        return NULL;
    }
    axis = joy_axis_from_name(joymap->joydev, name);
    if (axis == NULL) {
        parser_log_error("invalid axis name: '%s'", name);
        lib_free(name);
        return NULL;
    }

    /* axis direction */
    *direction = get_keyword();
    if (!kw_is_axis_direction(*direction)) {
        parser_log_error("expected axis direction ('negative' or 'positive')"
                         " after axis name, got '%s'",
                         pstate.curpos);
        lib_free(name);
        return NULL;
    }

    return axis;
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
    keyword_id_t   direction;

    axis = get_axis_and_direction(joymap, &direction);
    if (axis == NULL) {
        return NULL;
    }
    if (direction == VJM_KW_NEGATIVE) {
        mapping = &(axis->mapping.negative);
    } else {
        mapping = &(axis->mapping.positive);
    }

    msg_debug("name = %s, direction = %s\n", axis->name, kw_name(direction));
    return mapping;
}

static joy_calibration_t *get_axis_calibration(joymap_t *joymap)
{
    joy_axis_t   *axis;
    keyword_id_t  direction;

    axis = get_axis_and_direction(joymap, &direction);
    if (axis == NULL) {
        return NULL;
    } else if (direction == VJM_KW_NEGATIVE) {
        return &axis->calibration.negative;
    } else {
        return &axis->calibration.positive;
    }
}

/** \brief  Get button
 *
 * Parse current line and get button.
 *
 * \param[in]   joymap  joymap
 *
 * \return  button or \c NULL on error
 */
static joy_button_t *get_button(joymap_t *joymap)
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
    }
    lib_free(name);
    return button;
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

    button = get_button(joymap);
    if (button == NULL) {
        return NULL;
    }
    return &(button->mapping);
}

/** \brief  Get hat and direction
 *
 * Parse current line for hat name and direction.
 *
 * \param[in]   joymap      joymap
 * \param[out]  direction   hat direction
 *
 * \return  hat or \c NULL on error
 */
static joy_hat_t *get_hat_and_direction(joymap_t     *joymap,
                                        keyword_id_t *direction)
{
    joy_hat_t *hat;
    char      *name;

    /* hat name */
    if (!get_quoted_arg(&name)) {
        parser_log_error("expected hat name");
        return NULL;
    }
    hat = joy_hat_from_name(joymap->joydev, name);
    if (hat == NULL) {
        parser_log_error("invalid hat name: '%s'", name);
        lib_free(name);
        return NULL;
    }

    /* hat direction */
    *direction = get_keyword();
    if (!kw_is_joystick_direction(*direction)) {
        parser_log_error("invalid direction '%s', expected 'up', 'down', 'left'"
                         " or 'right'",
                         *direction == VJM_KW_INVALID ? pstate.curpos : kw_name(*direction));
        hat = NULL;
    }

    lib_free(name);
    return hat;
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
    joy_hat_t     *hat;
    joy_mapping_t *mapping   = NULL;
    keyword_id_t   direction = VJM_KW_INVALID;

    hat = get_hat_and_direction(joymap, &direction);

    switch (direction) {
        case VJM_KW_UP:
            mapping = &(hat->mapping.up);
            break;
        case VJM_KW_DOWN:
            mapping = &(hat->mapping.down);
            break;
        case VJM_KW_LEFT:
            mapping = &(hat->mapping.left);
            break;
        case VJM_KW_RIGHT:
            mapping = &(hat->mapping.right);
            break;
        default:
            parser_log_warning("shouldn't get here!");
            mapping = NULL;
            break;
    }
    return mapping;
}

/** \brief  Get mapping for either axis, button or hat
 *
 * Parse current line for "&lt;axis|button|hat&gt; \"&lt;name&gt;\"" and return
 * the associated mapping in \a joymap.
 *
 * \param[in]   joymap  joymap
 *
 * \return  mapping or \c NULL on error
 */
static joy_mapping_t *get_input_mapping(joymap_t *joymap)
{
    joy_mapping_t *mapping = NULL;

    switch (get_keyword()) {
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
            parser_log_error("expected input type ('axis', 'button' or 'hat')");
            break;
    }
    return mapping;
}

/** \brief  Handle pin mapping
 *
 * Parse current line for joystick pin mapping.
 * Called when encountering "map pin".
 *
 * \param[in]   joymap  joymap
 *
 * \return  \c true on success
 */
static bool handle_pin_mapping(joymap_t *joymap)
{
    int            pin;
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

    mapping = get_input_mapping(joymap);
    if (mapping != NULL) {
        msg_debug("mapping to pin %d\n", pin);
        mapping->action = JOY_ACTION_JOYSTICK;
        mapping->target.pin = pin;
        return true;
    } else {
        return false;
    }
}

/** \brief  Handle key mapping
 *
 * Parse current line for key press mapping.
 * Called when encountering "map key".
 *
 * \param[in]   joymap  joymap
 *
 * \return  \c true on success
 */
static bool handle_key_mapping(joymap_t *joymap)
{
    int           column;
    int           row;
    int           flags;
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

    mapping = get_input_mapping(joymap);
    if (mapping != NULL) {
        mapping->action            = JOY_ACTION_KEYBOARD;
        mapping->target.key.row    = row;
        mapping->target.key.column = column;
        mapping->target.key.flags  = (unsigned int)flags; 
        return true;
    } else {
        return false;
    }
}

/** \brief  Handle UI action mapping
 *
 * Parse line for UI action mapping. Calling when encountering "map action".
 *
 * \param[in]   joymap  joymap
 *
 * \return  \c true on success
 */
static bool handle_action_mapping(joymap_t *joymap)
{
    joy_mapping_t *mapping;
    int            action_id;
    char          *action_name = NULL;

    action_id = get_ui_action(&action_name);
    msg_debug("action name: %s, action id %d\n", action_name, action_id);
    lib_free(action_name);
    if (action_id < ACTION_NONE) {
        return false;
    }

    mapping = get_input_mapping(joymap);
    if (mapping != NULL) {
        mapping->action = JOY_ACTION_UI_ACTION;
        mapping->target.ui_action = action_id;
        return true;
    } else {
        return false;
    }
}

/** \brief  Handle mapping of host input to emulated input
 *
 * Parse line for different mapping types. Called when encountering the "map"
 * keyword.
 *
 * \param[in]   joymap  joymap
 *
 * \return  \c true on success
 */ 
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
            result = handle_action_mapping(joymap);
            break;
        default:
            parser_log_error("expected either 'pin', 'pot', 'key' or 'action'");
            result = false;
            break;
    }
    return result;
}

/** \brief  Parse line for axis calibration specification
 *
 * Parse current line for deadzone, fuzz and/or threshold specifications.
 *
 * \param[in]   joymap  joymap
 *
 * \return  \c true on success
 */
static bool handle_axis_calibration(joymap_t *joymap)
{
    joy_calibration_t *calibration;
    int                value;

    calibration = get_axis_calibration(joymap);
    if (calibration == NULL) {
        return false;
    }

    /* keep looking for "<keyword> <value>" pairs until end of line */
    while (*pstate.curpos != '\0') {
        switch (get_keyword()) {
            case VJM_KW_THRESHOLD:
                printf("%s(): got threshold keyword\n", __func__);
                if (!get_int_arg(&value)) {
                    parser_log_error("expected integer value for threshold");
                    return false;
                }
                calibration->threshold = (int32_t)value;
                break;
            case VJM_KW_DEADZONE:
                printf("%s(): got deadzone keyword\n", __func__);
                if (!get_int_arg(&value)) {
                    parser_log_error("expected integer value for deadzone");
                    return false;
                }
                calibration->deadzone = (int32_t)value;
                break;
            case VJM_KW_FUZZ:
                printf("%s(): got fuzz keyword\n", __func__);
                if (!get_int_arg(&value)) {
                    parser_log_error("expected integer value for fuzz");
                    return false;
                }
                calibration->fuzz = (int32_t)value;
                break;
            default:
                parser_log_error("expected either 'deadzone', 'fuzz' or 'threshold'");
                return false;
        }
    }

    printf("deadzone  = %d\n", calibration->deadzone);
    printf("fuzz      = %d\n", calibration->fuzz);
    printf("threshold = %d\n", calibration->threshold);

    return true;
}

/** \brief  Parse calibration specification on the current line
 *
 * \param[in]   joymap  joymap
 *
 * \return  \c true on success
 */
static bool handle_calibration(joymap_t *joymap)
{
    bool result = true;

    switch (get_keyword()) {
        case VJM_KW_AXIS:
            result = handle_axis_calibration(joymap);
            break;
        case VJM_KW_BUTTON:
            printf("Button calibration: is this a thing?\n");
            break;
        case VJM_KW_HAT:
            printf("Hat calibration\n");
            break;
        default:
            parser_log_error("expected input type ('axis', 'button' or 'hat')");
            result = false;
    }
    return result;
}


/** \brief  Handle initial keyword on current line
 *
 * Call keyword-specific handlers based on the initial keyword on the current
 * line.
 *
 * \param[in]   joymap  joymap
 * \param[in]   kw      keyword ID
 *
 * \return  \c on success
 */
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

        case VJM_KW_CALIBRATE:
            if (!handle_calibration(joymap)) {
                return false;
            }
            break;

        default:
            parser_log_error("unexpected keyword '%s'", kw_name(kw));
            result = false;
            break;
    }
    return result;
}

/** \brief  Parse current line
 *
 * Parse current line for header lines (vjm-version, vendor, product, name) or
 * mappings.
 *
 * \param[in]   joymap  joymap
 *
 * \return  \c true on success
 */
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

    pstate.path = path;

    msg_debug("loading joymap file '%s'\n", path);
    joymap = joymap_open(path);
    if (joymap == NULL) {
        return NULL;
    }
    joymap->joydev = joydev;

    errno          = 0;
    pstate.linenum = 0;
    while (joymap_read_line()) {
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


/** \brief  Initialize VJM parser module */
void joymap_module_init(void)
{
    pstate_init();
}


/** \brief  Shut down VJM parser module
 *
 * Clean up resources used by the parser.
 */
void joymap_module_shutdown(void)
{
    pstate_free();
}
