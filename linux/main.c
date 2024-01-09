#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "config.h"
#include "lib.h"
#include "cmdline.h"
#include "joyapi.h"


static bool  opt_verbose = false;


static const cmdline_opt_t options[] = {
    {   .type       = CMDLINE_BOOLEAN,
        .short_name = 'v',
        .long_name  = "verbose",
        .target     = &opt_verbose,
        .help       = "enable verbose messages",
    },

    CMDLINE_OPTIONS_END
};


int main(int argc, char **argv)
{
    char          **args;
    int            result;
    int            status = EXIT_SUCCESS;
    joy_device_t **devices = NULL;
    int            count;

    cmdline_init(PROGRAM_NAME, PROGRAM_VERSION);
    if (!cmdline_add_options(options)) {
        cmdline_free();
        return EXIT_FAILURE;
    }

    result = cmdline_parse(argc, argv, &args);
    if (result == CMDLINE_ERROR) {
        status = EXIT_FAILURE;
        goto cleanup;
    } else if (result == CMDLINE_HELP || result == CMDLINE_VERSION) {
        goto cleanup;
    }

    if (result > 0) {
        printf("non-option arguments:\n");
        for (int i = 0; i < result; i++) {
            printf("%d: %s\n", i, args[i]);
        }
    }
    printf("verbose = %s\n", opt_verbose ? "true" : "false");

    count = joy_get_devices(&devices);
    printf("found %d devices.\n", count);

cleanup:
    joy_free_devices(devices);
    cmdline_free();
    return status;
}
