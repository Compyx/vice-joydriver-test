#ifndef SHARED_CMDLINE_H
#define SHARED_CMDLINE_H

#include <stddef.h>
#include <stdbool.h>

/** \brief  Command line option types enum */
typedef enum {
     CMDLINE_BOOLEAN,
     CMDLINE_INTEGER,
     CMDLINE_STRING
} cmdline_type_t;

/** \brief  Status codes enum */
enum {
    CMDLINE_OK      =  0,   /**< OK */
    CMDLINE_ERROR   = -1,   /**< an error occured */
    CMDLINE_HELP    = -2,   /**< \c --help was handled */
    CMDLINE_VERSION = -3,   /**< \c --version was handled */
};

#define CMDLINE_OPTIONS_END { .short_name = 0, .long_name = NULL }

typedef struct {
    int             short_name;
    const char     *long_name;
    cmdline_type_t  type;
    const char     *help;
    const char     *param;
    void           *target;
} cmdline_opt_t;

void cmdline_init(const char *name, const char *version);
void cmdline_free(void);
void cmdline_show_help(void);
void cmdline_show_version(void);
bool cmdline_add_options(const cmdline_opt_t *options);
int  cmdline_parse(int argc, char **argv, char ***arglist);
#endif
