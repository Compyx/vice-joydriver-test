#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>
#ifdef WINDOWS_COMPILE
#include <windows.h>
#else
#include <signal.h>
#endif

#include "config.h"
#include "lib.h"
#include "cmdline.h"
#include "joyapi.h"

static bool  opt_verbose       = false;
static bool  opt_list_devices  = false;
static bool  opt_list_axes     = false;
static bool  opt_list_buttons  = false;
static bool  opt_list_hats     = false;
static char *opt_device_node   = NULL;
static bool  opt_poll_enable   = false;
static int   opt_poll_interval = 100;


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
    {   .type       = CMDLINE_STRING,
        .short_name = 'd',
        .long_name  = "device-node",
        .param      = "node-or-guid",
        .target     = &opt_device_node,
        .help       = "select device by node"
    },
    {   .type       = CMDLINE_BOOLEAN,
        .short_name = 'p',
        .long_name  = "poll",
        .target     = &opt_poll_enable,
        .help       = "start polling device"
    },
    {   .type       = CMDLINE_INTEGER,
        .short_name = 'i',
        .long_name  = "poll-interval",
        .target     = &opt_poll_interval,
        .param      = "msec",
        .help       = "specificy polling interval"
    },

    CMDLINE_OPTIONS_END
};


static joy_device_t **devices;


static void list_devices(int num_devices)
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


static joy_device_t *get_device(void)
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


static bool list_buttons(void)
{
    joy_device_t *joydev;
    unsigned int  b;

    if (opt_device_node == NULL) {
        fprintf(stderr,
                "%s: error: --list-buttons requires --device-node to be used.\n",
                cmdline_get_prg_name());
        return false;
    }

    joydev = get_device();
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


static bool list_axes(void)
{
    joy_device_t *joydev;
    unsigned int  a;

    if (opt_device_node == NULL) {
        fprintf(stderr,
                "%s: error: --list-axes requires --device-node to be used.\n",
                cmdline_get_prg_name());
        return false;
    }

    joydev = get_device();
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

static bool list_hats(void)
{
    joy_device_t *joydev;
    unsigned int  h;

    if (opt_device_node == NULL) {
        fprintf(stderr,
                "%s: error: --list-hats requires --device-node to be used.\n",
                cmdline_get_prg_name());
        return false;
    }

    joydev = get_device();
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


/** \brief  Flag to stop polling
 *
 * If set to \c true polling is stopped.
 */
static bool stop_polling = false;

#ifdef WINDOWS_COMPILE
static BOOL consoleHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT) {
        stop_polling = true;
    }
    return TRUE;
}
#else
static void sig_handler(int s)
{
    if (s == SIGINT) {
        stop_polling = true;
    }
}
#endif


static int poll_loop(void)
{
    joy_device_t    *joydev;
    struct timespec  spec;
#ifndef WINDOWS_COMPILE
    struct sigaction action = { 0 };
#endif

    if (opt_device_node == NULL) {
        fprintf(stderr, "%s: --poll requires --device-node to be used.\n",
                cmdline_get_prg_name());
        return EXIT_FAILURE;
    }

    joydev = get_device();
    if (joydev == NULL) {
        return EXIT_FAILURE;
    }

    /* amazingly nanosleep() is available on Windows (msys2) */
    spec.tv_sec  = opt_poll_interval / 1000;
    spec.tv_nsec = (opt_poll_interval % 1000) * 1000000;
    stop_polling = false;

#ifdef WINDOWS_COMPILE
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
    action.sa_handler = sig_handler;
    sigaction(SIGINT, &action, NULL);
#endif

    while (true) {
        if (!joy_poll(joydev)) {
            return EXIT_FAILURE;
        }
        if (stop_polling) {
            printf("Caught SIGINT, stopping polling\n");
            return EXIT_SUCCESS;
        }
        if (opt_poll_interval > 0) {
            nanosleep(&spec, NULL);
        }
    }
    return EXIT_SUCCESS;
}


int main(int argc, char **argv)
{
    char **args;
    int    result;
    int    status = EXIT_SUCCESS;
    int    count;

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

    /* initialize arch-specific joy system */
    joy_init();

    /* enumerate connected devices */
    count = joy_device_list_init(&devices);
    if (count == 0) {
        printf("No devices found.\n");
    } else if (count == 1) {
        printf("Found 1 device.\n");
    } else if (count > 1) {
        printf("Found %d devices.\n", count);
    } else {
        printf("%s: error querying devices.\n", cmdline_get_prg_name());
        status = EXIT_FAILURE;
        goto cleanup;
    }
    if (opt_poll_enable) {
        status = poll_loop();
    } else if (opt_list_devices && count > 0) {
        list_devices(count);
    } else {
        if (opt_list_buttons) {
            if (!list_buttons()) {
                status = EXIT_FAILURE;
                goto cleanup;
            }
        }
        if (opt_list_axes) {
            if (!list_axes()) {
                status = EXIT_FAILURE;
                goto cleanup;
            }
        }
        if (opt_list_hats) {
            if (!list_hats()) {
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
