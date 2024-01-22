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


/** \brief  List of joystick devices found */
static joy_device_t **devices;
/** \brief  Number of joystick devices found */
static int devcount;
/** \brief  Non-option arguments (devices to list/poll) */
static char **args;
/** \brief  Number of non-option arguments */
static int argcount;


/** \brief  Check if we have at least one device node/GUID
 *
 * Check non-option arg count, print error message when no non-option arguments
 * were specified.
 *
 * \param[in]   optname option name (without leading --) for error message
 *
 * \return  \c true if arg count > 0
 */
static bool has_required_args(const char *optname)
{
    if (argcount == 0) {
        fprintf(stderr, "%s: error: --%s requires at least one device node.\n",
                cmdline_get_prg_name(), optname);
        return false;
    }
    return true;
}

/** \brief  List buttons for requested devices
 *
 * \return  \c true on success
 */
static bool list_buttons(void)
{
    if (!has_required_args("list-buttons")) {
        return false;
    }

    for (int i = 0; i < argcount; i++) {
        joy_device_t *joydev = joy_device_get(devices, args[i]);

        if (joydev == NULL) {
            fprintf(stderr,
                    "%s: error: failed to find device %s, skipping.\n",
                    cmdline_get_prg_name(), args[i]);
        } else if (joydev->num_buttons == 0) {
            printf("No buttons for device found.\n");
        } else {
            printf("Buttons for device %s (%s):\n", args[i], joydev->name);
            for (uint32_t b = 0; b < joydev->num_buttons; b++) {
                joy_button_t *button = &(joydev->buttons[b]);

                printf("%2u: code: %0x4, name: %s\n", b, button->code, button->name);
            }
        }
    }
    return true;
}

/** \brief  List axes for requested devices
 *
 * \return  \c true on success
 */
static bool list_axes(void)
{

    if (!has_required_args("list-axes")) {
        return false;
    }

    for (int i = 0; i < argcount; i++) {
        joy_device_t *joydev = joy_device_get(devices, args[i]);

        if (joydev == NULL) {
            fprintf(stderr,
                    "%s: error: failed to find device %s, skipping.\n",
                    cmdline_get_prg_name(), args[i]);
        } else  if (joydev->num_axes == 0) {
            printf("No axes for device found.\n");
        } else {
            printf("Axes for device %s (%s):\n", args[i], joydev->name);
            for (uint32_t a = 0; a < joydev->num_axes; a++) {
                joy_axis_t *axis = &joydev->axes[a];

                printf("%2u: code: %04x, name: %s, range: %"PRId32" - %"PRId32"\n",
                       a, axis->code, axis->name, axis->minimum, axis->maximum);
            }
        }
    }
    return true;
}

/** \brief  List hats for requested devices
 *
 * \return  \c true on success
 */
static bool list_hats(void)
{
    if (!has_required_args("list-hats")) {
        return false;
    }

    for (int i = 0; i < argcount; i++) {
        joy_device_t *joydev = joy_device_get(devices, args[i]);

        if (joydev == NULL) {
            fprintf(stderr,
                    "%s: error: failed to find device %s, skipping.\n",
                    cmdline_get_prg_name(), args[i]);
        } else  if (joydev->num_hats == 0) {
            printf("No hats for device found.\n");
        } else {
            printf("Hats for device %s (%s):\n", args[i], joydev->name);

            for (uint32_t h = 0; h < joydev->num_hats; h++) {
                joy_hat_t *hat = &(joydev->hats[h]);
                joy_axis_t *x = &(hat->x);
                joy_axis_t *y = &(hat->y);

                printf("%2x: name: %s\n", h, hat->name);
                printf("    X axis: code: %04x, name: %s, range: %"PRId32" - %"PRId32"\n",
                       x->code, x->name, x->minimum, x->maximum);
                printf("    Y axis: code: %04x, name: %s, range: %"PRId32" - %"PRId32"\n",
                       y->code, y->name, y->minimum, y->maximum);

            }
        }
    }
    return true;
}

/** \brief  List devices found */
static void list_devices(void)
{
    for (int i = 0; i < devcount; i++) {
        if (opt_verbose) {
            printf("device %d:\n", i);
        }
        joy_device_dump(devices[i], opt_verbose);
        if (opt_verbose) {
            putchar('\n');
        }
    }
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
    int status = EXIT_SUCCESS;

    if (argcount == 0) {
        fprintf(stderr, "%s: --poll requires at least one device node.\n",
                cmdline_get_prg_name());
        return EXIT_FAILURE;
    }

    /* just the first argument for now */
    joydev = joy_device_get(devices, args[0]);
    if (joydev == NULL) {
        return EXIT_FAILURE;
    }

    if (!joy_open(joydev)) {
        fprintf(stderr,
                "%s: failed to open device %s.\n",
                cmdline_get_prg_name(), args[0]);
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
            status = EXIT_FAILURE;
            goto poll_exit;
        }
        if (stop_polling) {
            printf("Caught SIGINT, stopping polling\n");
            status = EXIT_SUCCESS;
            goto poll_exit;
        }
        if (opt_poll_interval > 0) {
            nanosleep(&spec, NULL);
        }
    }

poll_exit:
    joy_close(joydev);
    return status;
}


int main(int argc, char **argv)
{
    int status = EXIT_SUCCESS;

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

    argcount = cmdline_parse(argc, argv, &args);
    if (argcount == CMDLINE_ERROR) {
        status = EXIT_FAILURE;
        goto cleanup;
    } else if (argcount == CMDLINE_HELP || argcount == CMDLINE_VERSION) {
        goto cleanup;
    }

    printf("OS: " OSNAME "\n");

    /* initialize arch-specific joy system */
    joy_init();

    /* enumerate connected devices */
    devcount = joy_device_list_init(&devices);
    if (devcount == 0) {
        printf("No devices found.\n");
    } else if (devcount == 1) {
        printf("Found 1 device.\n");
    } else if (devcount > 1) {
        printf("Found %d devices.\n", devcount);
    } else {
        printf("%s: error querying devices.\n", cmdline_get_prg_name());
        status = EXIT_FAILURE;
        goto cleanup;
    }

    if (opt_poll_enable) {
        status = poll_loop();
    } else if (opt_list_devices && devcount > 0) {
        list_devices();
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
    joy_device_list_free(devices);
    cmdline_free();
    return status;
}
