#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

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



static void list_devices(joy_device_t **devices, int num_devices)
{
    for (int i = 0; i < num_devices; i++) {
        if (opt_verbose) {
            printf("device %d:\n", i);
        }
        joy_device_dump(devices[i], opt_verbose);
        if (opt_verbose) {
            putchar('\n');
        }
    }
}


static joy_device_t *get_device(joy_device_t **devices)
{
    joy_device_t *joydev = joy_device_get(devices, opt_device_node);
    if (joydev == NULL) {
        fprintf(stderr,
                "%s: error: could not find device '%s'.\n",
                cmdline_get_prg_name(), opt_device_node);
        return NULL;
    }
    return joydev;
}


static bool list_buttons(joy_device_t **devices)
{
    joy_device_t *joydev;
    unsigned int  b;

    if (opt_device_node == NULL) {
        fprintf(stderr,
                "%s: error: --list-buttons requires --device-node to be used.\n",
                cmdline_get_prg_name());
        return false;
    }

    joydev = get_device(devices);
    if (joydev == NULL) {
        return false;
    }

    if (joydev->num_buttons == 0) {
        printf("No buttons for device found.\n");
        return true;
    }
    printf("Buttons:\n");
    for (b = 0; b < joydev->num_buttons; b++) {
        joy_button_t *button = &(joydev->buttons[b]);

        printf("%2u: code: %0x4, name: %s\n", b, button->code, button->name);
    }
    return true;
}


static bool list_axes(joy_device_t **devices)
{
    joy_device_t *joydev;
    unsigned int  a;

    if (opt_device_node == NULL) {
        fprintf(stderr,
                "%s: error: --list-axes requires --device-node to be used.\n",
                cmdline_get_prg_name());
        return false;
    }

    joydev = get_device(devices);
    if (joydev == NULL) {
        return false;
    }

    if (joydev->num_axes == 0) {
        printf("No axes for device found.\n");
        return true;
    }
    printf("Axes:\n");
    for (a = 0; a < joydev->num_axes; a++) {
        joy_axis_t *axis = &joydev->axes[a];

        printf("%2u: code: %04x, name: %s, range: %"PRId32" - %"PRId32"\n",
                a, axis->code, axis->name, axis->minimum, axis->maximum);

    }
    return true;
}

static bool list_hats(joy_device_t **devices)
{
    joy_device_t *joydev;
    unsigned int  h;

    if (opt_device_node == NULL) {
        fprintf(stderr,
                "%s: error: --list-hats requires --device-node to be used.\n",
                cmdline_get_prg_name());
        return false;
    }

    joydev = get_device(devices);
    if (joydev == NULL) {
        return false;
    }

    if (joydev->num_hats == 0) {
        printf("No hats for device found.\n");
        return true;
    }
    printf("Hats:\n");
    for (h = 0; h < joydev->num_hats; h++) {
        joy_hat_t *hat = &(joydev->hats[h]);
        joy_axis_t *x = &(hat->x);
        joy_axis_t *y = &(hat->y);

        printf("%2x: name: %s\n", h, hat->name);
        printf("    X axis: code: %04x, name: %s, range: %"PRId32" - %"PRId32"\n",
               x->code, x->name, x->minimum, x->maximum);
        printf("    Y axis: code: %04x, name: %s, range: %"PRId32" - %"PRId32"\n",
               y->code, y->name, y->minimum, y->maximum);

    }
    return true;
}




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

    printf("OS: " OSNAME "\n");

#if 0
    if (result > 0) {
        printf("non-option arguments:\n");
        for (int i = 0; i < result; i++) {
            printf("%d: %s\n", i, args[i]);
        }
    }
#endif
//    printf("verbose = %s\n", opt_verbose ? "true" : "false");

    count = joy_device_list_init(&devices);
    if (count == 0) {
        printf("No devices found.\n");
    } else if (count == 1) {
        printf("Found 1 device:\n");
    } else if (count > 1) {
        printf("Found %d devices.\n", count);
    } else {
        printf("%s: error querying devices.\n", cmdline_get_prg_name());
        status = EXIT_FAILURE;
        goto cleanup;
    }
    if (opt_list_devices && count > 0){
        list_devices(devices, count);
    } else {
        if (opt_list_buttons) {
            if (!list_buttons(devices)) {
                status = EXIT_FAILURE;
                goto cleanup;
            }
        }
        if (opt_list_axes) {
            if (!list_axes(devices)) {
                status = EXIT_FAILURE;
                goto cleanup;
            }
        }
        if (opt_list_hats) {
            if (!list_hats(devices)) {
                status = EXIT_FAILURE;
                goto cleanup;
            }
        }

    }

cleanup:
    lib_free(opt_device_node);
    joy_device_list_free(devices);
    cmdline_free();
    return status;
}
