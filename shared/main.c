#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "config.h"
#include "lib.h"
#include "cmdline.h"
#include "joyapi.h"


static bool  opt_verbose      = false;
static bool  opt_list_devices = false;
static bool  opt_list_axes    = false;
static bool  opt_list_buttons = false;
static bool  opt_list_hats    = false;
static char *opt_device_node  = NULL;


static const cmdline_opt_t options[] = {
    {   .type       = CMDLINE_BOOLEAN,
        .short_name = 'v',
        .long_name  = "verbose",
        .target     = &opt_verbose,
        .help       = "enable verbose messages",
    },
    {   .type       = CMDLINE_BOOLEAN,
        .long_name  = "list-devices",
        .target     = &opt_list_devices,
        .help       = "list all joystick devices"
    },
    {   .type       = CMDLINE_STRING,
        .short_name = 'd',
        .long_name  = "device-node",
        .param      = "NODE-or-GUID",
        .target     = &opt_device_node,
        .help       = "select device by node"
    },
    {   .type       = CMDLINE_BOOLEAN,
        .long_name  = "list-axes",
        .target     = &opt_list_axes,
        .help       = "list axes of a device"
    },
    {   .type       = CMDLINE_BOOLEAN,
        .long_name  = "list-buttons",
        .target     = &opt_list_buttons,
        .help       = "list buttons of a device"
    },
    {   .type       = CMDLINE_BOOLEAN,
        .long_name  = "list-hats",
        .target     = &opt_list_hats,
        .help       = "list hats of a device"
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

    if (argc < 2) {
        cmdline_show_help();
        cmdline_free();
        return EXIT_SUCCESS;
    }

    result = cmdline_parse(argc, argv, &args);
    if (result == CMDLINE_ERROR) {
        status = EXIT_FAILURE;
        goto cleanup;
    } else if (result == CMDLINE_HELP || result == CMDLINE_VERSION) {
        goto cleanup;
    }

#if defined(LINUX_COMPILE)
    printf("OS: Linux.\n");
#elif defined(WINDOWS_COMPILE)
    printf("OS: Windows.\n");
#else
    printf("OS: not detected.\n");
#endif

#if 0
    if (result > 0) {
        printf("non-option arguments:\n");
        for (int i = 0; i < result; i++) {
            printf("%d: %s\n", i, args[i]);
        }
    }
#endif
//    printf("verbose = %s\n", opt_verbose ? "true" : "false");

    count = joy_get_devices(&devices);
    if (count == 0) {
        printf("No devices found.\n");
    } else if (count == 1) {
        printf("Found 1 device:\n");
    } else if (count > 1) {
        printf("Found %d devices.\n", count);
    } else {
        printf("error querying devices.\n");
        status = EXIT_FAILURE;
    }
    if (opt_list_devices && count > 0){
        for (int i = 0; i < count; i++) {
            if (opt_verbose) {
                printf("device %d:\n", i);
            }
            joy_device_dump(devices[i], opt_verbose);
            if (opt_verbose) {
                printf("----\n");
            }
        }
    }

cleanup:
    lib_free(opt_device_node);
    joy_free_devices(devices);
    cmdline_free();
    return status;
}
