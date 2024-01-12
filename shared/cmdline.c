/** \file   cmdline.c
 * \brief   Simple command line parser
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 *
 * Command line option parser that supports short and long options.
 * Combining multiple short options into a single `-xyz` form is not supported.
 * Combining options and argument in a single argv[] element is supported:
 * For example '-v3' or '--verbose=3'.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "lib.h"

#include "cmdline.h"


#define OPTS_INITIAL_SIZE 16u
#define ARGS_INITIAL_SIZE 16u

static char *prg_name;
static char *prg_version;

static const cmdline_opt_t **opts_list;
static size_t                opts_list_count;
static size_t                opts_list_size = OPTS_INITIAL_SIZE;

static char                **args_list;
size_t                       args_list_count;
size_t                       args_list_size;


static void args_list_init(void)
{
    args_list_count = 0;
    args_list_size  = ARGS_INITIAL_SIZE;
    args_list       = lib_malloc(ARGS_INITIAL_SIZE * sizeof *args_list);
    args_list[0]    = NULL;
}

static void args_list_append(const char *s)
{
    if (args_list_count == (args_list_size - 1u)) {
        args_list_size *= 2u;
        args_list = lib_realloc(args_list, sizeof *args_list * args_list_size);
    }
    args_list[args_list_count++] = lib_strdup(s);
    args_list[args_list_count] = NULL;
}

static void args_list_free(void)
{
    for (size_t i = 0; i < args_list_count; i++) {
        lib_free(args_list[i]);
    }
    lib_free(args_list);
}


static void opts_list_init(void)
{
    opts_list_count = 0;
    opts_list_size  = OPTS_INITIAL_SIZE;
    opts_list       = lib_malloc(OPTS_INITIAL_SIZE * sizeof *opts_list);
}

static void opts_list_free(void)
{
    lib_free(opts_list);
}


static const cmdline_opt_t *find_short_option(int name)
{
    if (name > 0) {
        for (size_t i = 0; i < opts_list_count; i++) {
            const cmdline_opt_t *opt = opts_list[i];

            if (opt->short_name == name) {
                return opt;
            }
        }
    }
    return NULL;
}

static const cmdline_opt_t *find_long_option(const char *name)
{
    if (name != NULL && *name != '\0') {
        for (size_t i = 0; i < opts_list_count; i++) {
            const cmdline_opt_t *opt = opts_list[i];

            if (strcmp(opt->long_name, name) == 0) {
                return opt;
            }
        }
    }
    return NULL;
}



static bool handle_option(const cmdline_opt_t *option, const char *arg, bool is_short)
{
    bool  result = true;
    char *endptr = NULL;
    long  value;

    switch (option->type) {

        case CMDLINE_BOOLEAN:
            *(bool *)(option->target) = true;
            break;

        case CMDLINE_INTEGER:
            if (arg == NULL || *arg == '\0') {
                result = false;
                if (is_short) {
                    fprintf(stderr,
                            "%s: error: missing argument for option '-%c'.\n",
                            prg_name, option->short_name);
                } else {
                    fprintf(stderr,
                            "%s: error: missing argument for option '--%s'.\n",
                            prg_name, option->long_name);
                }
                break;
            }

            value = strtol(arg, &endptr, 0);
            if (*endptr != '\0') {
                result = false;
                if (is_short) {
                    fprintf(stderr,
                            "%s: failed to parse integer value for option '-%c'.\n",
                            prg_name, option->short_name);
                } else {
                    fprintf(stderr,
                            "%s: failed to parse integer value for option '--%s'.\n",
                            prg_name, option->long_name);
                }
            } else {
                *(int *)(option->target) = (int)value;
            }
            break;

        case CMDLINE_STRING:
            *(char **)(option->target) = lib_strdup(arg);
            break;

        default:
            fprintf(stderr, "%s(): unhandled option type %u.\n", __func__, option->type);
            result = false;
    }

    return result;
}


static int handle_short_option(const char *arg1, const char *arg2)
{
    const cmdline_opt_t *option;
    int                  delta;

    option = find_short_option(arg1[1]);
    if (option == NULL) {
        fprintf(stderr, "%s: error: unknown option '-%c'.\n", prg_name, arg1[1]);
        return CMDLINE_ERROR;
    }

    if (arg1[2] != '\0') {
        if (option->type == CMDLINE_BOOLEAN) {
            fprintf(stderr, "%s: error: option '-%c' does not take an argument.\n",
                    prg_name, arg1[1]);
            return CMDLINE_ERROR;
        }
        arg2 = arg1 + 2;
        delta = 0;
    } else {
        if (option->type == CMDLINE_BOOLEAN) {
            delta = 0;
        } else {
            delta = 1;
        }
    }

    if (!handle_option(option, arg2, true)) {
        return CMDLINE_ERROR;
    }
    return delta;
}

static int handle_long_option(const char *arg1, const char *arg2)
{
    const cmdline_opt_t *option;
    int                  delta;
    char                *name;
    char                *assign;

    assign = strchr(arg1, '=');
    if (assign != NULL) {
        name  = lib_strndup(arg1 + 2, (size_t)(assign - arg1 - 2));
        arg2  = assign + 1;
    } else {
        name  = lib_strdup(arg1 + 2);
    }

    option = find_long_option(name);
    if (option == NULL) {
        fprintf(stderr, "%s: error: unknown option '--%s'.\n", prg_name, name);
        lib_free(name);
        return CMDLINE_ERROR;
    }
    if (assign != NULL && option->type == CMDLINE_BOOLEAN) {
        fprintf(stderr, "%s: error: option '--%s' does not take an argument.\n",
                prg_name, name);
        lib_free(name);
        return CMDLINE_ERROR;
    }

    if (assign != NULL || option->type == CMDLINE_BOOLEAN) {
        delta = 0;
    } else {
        delta = 1;
    }

    lib_free(name);

    if (!handle_option(option, arg2, false)) {
        return CMDLINE_ERROR;
    }
    return delta;
}

void cmdline_init(const char *name, const char *version)
{
    prg_name    = lib_strdup(name);
    prg_version = lib_strdup(version);

    args_list_init();
    opts_list_init();
}


void cmdline_free(void)
{
    lib_free(prg_name);
    lib_free(prg_version);

    args_list_free();
    opts_list_free();
}


bool cmdline_add_options(const cmdline_opt_t *options)
{
    const cmdline_opt_t *opt = options;

    while (opt->short_name > 0 || (opt->long_name != NULL && opt->long_name[0] != '\0')) {
        /* check for duplicate name */
        if (find_short_option(opt->short_name) != NULL) {
            fprintf(stderr, "%s: error: option '-%c' already registered.\n",
                    prg_name, opt->short_name);
            return false;
        }
        if (find_long_option(opt->long_name) != NULL) {
            fprintf(stderr, "%s: error: option '--%s' already registered.\n",
                    prg_name, opt->long_name);
            return false;
        }

        /* check for target */
        if ((opt->type == CMDLINE_INTEGER || opt->type == CMDLINE_STRING) &&
                opt->target == NULL) {
            if (opt->short_name > 0) {
                fprintf(stderr, "%s: error: `target` is NULL for option '-%c'.\n",
                        prg_name, opt->short_name);
                return false;
            } else {
                fprintf(stderr, "%s: error: `target` is NULL for option '--%s.\n",
                        prg_name, opt->long_name);
                return false;
            }
        }

        /* make sure we have space for the new option */
        if (opts_list_count == opts_list_size) {
            opts_list_size *= 2u;
            opts_list = lib_realloc(opts_list, opts_list_size * sizeof *opts_list);
        }

        opts_list[opts_list_count++] = opt;
        opt++;
    }
    return true;
}


void cmdline_show_help(void)
{
    printf("%s :: help\n\n", prg_name);

    printf("  -h, --help                            show help\n");
    printf("      --version                         show program version\n\n");

    for (size_t i = 0; i < opts_list_count; i++) {
        const cmdline_opt_t *opt = opts_list[i];
        int                  col;

        if (opt->short_name > 0 && opt->long_name != NULL) {
            col = printf("  -%c, --%s", opt->short_name, opt->long_name);
        } else if (opt->short_name > 0) {
            col = printf("  %-c", opt->short_name);
        } else {
            col = printf("      --%s", opt->long_name);
        }
        if (opt->type == CMDLINE_INTEGER || opt->type == CMDLINE_STRING) {
            col += printf(" <%s>", opt->param != NULL ? opt->param : "ARG");
        }
        while (col++ < 40) {
            putchar(' ');
        }
        printf("%s\n", opt->help);
    }
}


void cmdline_show_version(void)
{
    printf("%s %s\n", prg_name, prg_version);
}


int cmdline_parse(int argc, char **argv, char ***arglist)
{
    int i;

    for (i = 1; i < argc; i++) {
        // printf("%s(): parsing '%s'\n", __func__, argv[i]);
        if ((strcmp(argv[i], "--help") == 0) || (strcmp(argv[i], "-h") == 0)) {
            cmdline_show_help();
            return CMDLINE_HELP;
        }
        if (strcmp(argv[i], "--version") == 0) {
            cmdline_show_version();
            return CMDLINE_VERSION;
        }

        if (argv[i][0] != '-') {
            // printf("%s(): argument '%s'\n", __func__, argv[i]);
            args_list_append(argv[i]);
        } else {
            int delta;

            // printf("%s(): possible option '%s'\n", __func__, argv[i]);
            if (argv[i][1] == '-') {
                delta = handle_long_option(argv[i], argv[i + 1]);
            } else {
                delta = handle_short_option(argv[i], argv[i + 1]);
            }
            if (delta < 0) {
                return CMDLINE_ERROR;
            }

            // printf("%s(): delta = %d\n", __func__, delta);
            i += delta;
        }
    }
    if (arglist != NULL) {
        *arglist = args_list;
    }
    return (int)args_list_count;
}
