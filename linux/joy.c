/** \file   joy.c
 * \brief   Linux joystick interface
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <inttypes.h>

#include <dirent.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "joyapi.h"
#include "lib.h"


extern bool debug;
extern bool verbose;


#define DEVICES_INITIAL_SIZE    16
#define BUTTONS_INITIAL_SIZE    32
#define AXES_INITIAL_SIZE       16
#define HATS_INITIAL_SIZE       4

#define NODE_ROOT               "/dev/input"
#define NODE_ROOT_LEN           10

#define NODE_PREFIX             "event"
#define NODE_PREFIX_LEN         5


/** \brief  Hat event codes for both axes
 */
typedef struct {
    uint16_t x; /* X axis */
    uint16_t y; /* Y axis */
} hat_evcode_t;

/** \brief  Mapping event codes to names */
typedef struct {
    unsigned int  code;
    const char   *name;
} ev_code_name_t;

/** \brief  Private data object
 *
 * Allocated during device detection, used in the \c open(), \c poll() and
 * \c close() driver callbacks, freed via the driver's \c priv_free() callback.
 */
typedef struct joy_priv_s {
    struct libevdev *evdev;     /**< evdev instance */
    int              fd;        /**< file descriptor */
} joy_priv_t;


/* The XBox "profile" axis is a recent addition, from kernel ~6.1 onward, so we
 * define it ourselves here.
 * See https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/commit/?h=v6.1.56&id=1260cd04a601e0e02e09fa332111b8639611970d
 */
#ifndef ABS_PROFILE
#define ABS_PROFILE 0x21
#endif


/** \brief  Test if an event code is an axis event code
 *
 * \param[in]   code    event code
 * \return  \c true if axis
 */
#define IS_AXIS(code) (((code) >= ABS_X && (code) < ABS_HAT0X) || ((code) >= ABS_PRESSURE && (code) <= ABS_MISC))

/** \brief  Test if an event code is a button event code
 *
 * \param[in]   code    event code
 * \return  \c true if button
 */
#define IS_BUTTON(code) ((code) >= BTN_JOYSTICK && (code) <= BTN_THUMBR)

/** \brief  Test if an event code is a hat axis code
 *
 * \param[in]   code    event code
 * \return  \c true if hat
 */
#define IS_HAT(code) ((code) >= ABS_HAT0X) && ((code) <= ABS_HAT3Y)


/** \brief  Button names */
static const ev_code_name_t button_names[] = {
    /* 0x100-0x109 - BTN_MISC */
    { BTN_0,        "Btn0" },           { BTN_1,        "Btn1" },
    { BTN_0,        "Btn2" },           { BTN_1,        "Btn3" },
    { BTN_0,        "Btn4" },           { BTN_1,        "Btn5" },
    { BTN_0,        "Btn6" },           { BTN_1,        "Btn7" },
    { BTN_0,        "Btn8" },           { BTN_1,        "Btn9" },

    /* 0x110-0x117 - BTN_MOUSE */
    { BTN_LEFT,     "LeftBtn" },        { BTN_RIGHT,    "RightBtn" },
    { BTN_MIDDLE,   "MiddleBtn" },      { BTN_SIDE,     "SideBtn" },
    { BTN_EXTRA,    "ExtraBtn" },       { BTN_FORWARD,  "FowardBtn" },
    { BTN_BACK,     "BackBtn" },        { BTN_TASK,     "TaskBtn" },

    /* 0x120-0x12f - BTN_JOYSTICK */
    { BTN_TRIGGER,  "Trigger" },        { BTN_THUMB,    "ThumbBtn" },
    { BTN_THUMB2,   "ThumbBtn2" },      { BTN_TOP,      "TopBtn" },
    { BTN_TOP2,     "TopBtn2" },        { BTN_PINKIE,   "PinkieButton" },
    { BTN_BASE,     "BaseBtn" },        { BTN_BASE2,    "BaseBtn2" },
    { BTN_BASE3,    "BaseBtn3" },       { BTN_BASE4,    "BaseBtn4" },
    { BTN_BASE5,    "BaseBtn5" },       { BTN_BASE6,    "BaseBtn6" },
    { BTN_DEAD,     "BtnDead" },

    /* 0x130-0x13e - BTN_GAMEPAD */
    { BTN_A,        "BtnA" },           { BTN_B,        "BtnB" },
    { BTN_C,        "BtnC" },           { BTN_X,        "BtnX" },
    { BTN_Y,        "BtnY" },           { BTN_Z,        "BtnZ" },
    { BTN_TL,       "BtnTL" },          { BTN_TR,       "BtnTR" },
    { BTN_TL2,      "BtnTL2" },         { BTN_TR2,      "BtnTR2" },
    { BTN_SELECT,   "BtnSelect" },      { BTN_START,    "BtnStart" },
    { BTN_MODE,     "BtnMode" },        { BTN_THUMBL,   "BtnThumbL" },
    { BTN_THUMBR,   "BtnThumbR" },

    /* Skipping 0x140-0x14f: BTN_TOOL_PEN - BTN_TOOL_QUADTAP */

    /* 0x150-0x151: BTN_WHEEL */
    { BTN_GEAR_DOWN, "GearDown" },      { BTN_GEAR_UP,  "GearUp" },

    /* 0x220-0223 */
    { BTN_DPAD_UP,   "BtnDPadUp" },     { BTN_DPAD_DOWN,  "BtnDPadDown" },
    { BTN_DPAD_LEFT, "BtnDPadLeft" },   { BTN_DPAD_RIGHT, "BtnDPadRight" }
};

/** \brief  Axis names */
static const ev_code_name_t axis_names[] = {
    { ABS_X,            "X" },
    { ABS_Y,            "Y" },
    { ABS_Z,            "Z" },
    { ABS_RX,           "Rx" },
    { ABS_RY,           "Ry" },
    { ABS_RZ,           "Rz" },
    { ABS_THROTTLE,     "Throttle" },
    { ABS_RUDDER,       "Rudder" },
    { ABS_WHEEL,        "Wheel" },
    { ABS_GAS,          "Gas" },
    { ABS_BRAKE,        "Brake" },
    { ABS_HAT0X,        "Hat0X" },
    { ABS_HAT0Y,        "Hat0Y" },
    { ABS_HAT1X,        "Hat1X" },
    { ABS_HAT1Y,        "Hat1Y" },
    { ABS_HAT2X,        "Hat2X" },
    { ABS_HAT2Y,        "Hat2Y" },
    { ABS_HAT3X,        "Hat3X" },
    { ABS_HAT3Y,        "Hat3Y" },
    { ABS_PRESSURE,     "Pressure" },
    { ABS_DISTANCE,     "Distance" },
    { ABS_TILT_X,       "XTilt" },
    { ABS_TILT_Y,       "YTilt" },
    { ABS_TOOL_WIDTH,   "ToolWidth" },
    { ABS_VOLUME,       "Volume" },
    { ABS_PROFILE,      "Profile" },
    { ABS_MISC,         "Misc" }
};

/** \brief  Hat names
 *
 * Each of two hat axes maps to the same name.
 */
static const ev_code_name_t hat_names[] = {
    { ABS_HAT0X,    "Hat0" },
    { ABS_HAT0Y,    "Hat0" },
    { ABS_HAT1X,    "Hat1" },
    { ABS_HAT1Y,    "Hat1" },
    { ABS_HAT2X,    "Hat2" },
    { ABS_HAT2Y,    "Hat2" },
    { ABS_HAT3X,    "Hat3" },
    { ABS_HAT3Y,    "Hat3" },
};

/** \brief X and Y axes for the four hats found in `linux/input-event-codes.h` */
static const hat_evcode_t hat_event_codes[] = {
    { ABS_HAT0X, ABS_HAT0Y },
    { ABS_HAT1X, ABS_HAT1Y },
    { ABS_HAT2X, ABS_HAT2Y },
    { ABS_HAT3X, ABS_HAT3Y }
};


static joy_priv_t *joydev_priv_new(void)
{
    joy_priv_t *priv = lib_malloc(sizeof *priv);

    priv->evdev = NULL;
    priv->fd    = -1;
    return priv;
}

static void joydev_priv_free(void *priv)
{
    joy_priv_t *pr = priv;

    if (pr != NULL) {
        if (pr->evdev) {
            libevdev_free(pr->evdev);
        }
        if (pr->fd >= 0) {
            close(pr->fd);
        }
    }
    lib_free(priv);
}

static const char *get_event_code_name(const ev_code_name_t *list,
                                       size_t                length,
                                       unsigned int          code)
{
    for (size_t i = 0; i < length; i++) {
        if (list[i].code == code) {
            return list[i].name;
        } else if (list[i].code > code) {
            break;
        }
    }
    return NULL;
}

static const char *get_axis_name(unsigned int code)
{
    return get_event_code_name(axis_names, ARRAY_LEN(axis_names), code);
}

static const char *get_button_name(unsigned int code)
{
    return get_event_code_name(button_names, ARRAY_LEN(button_names), code);
}

static const char *get_hat_name(unsigned int code)
{
    return get_event_code_name(hat_names, ARRAY_LEN(hat_names), code);
}

static int node_filter(const struct dirent *de)
{
    const char *name;
    size_t      len;

    name = de->d_name;
    len  = strlen(name);

    if (len > NODE_PREFIX_LEN && memcmp(name, NODE_PREFIX, NODE_PREFIX_LEN) == 0) {
        return 1;
    }
    return 0;
}

static char *node_full_path(const char *node)
{
    size_t  nlen;
    char   *path;

    nlen = strlen(node);
    path = lib_malloc(NODE_ROOT_LEN + 1u + nlen + 1u);
    memcpy(path, NODE_ROOT, NODE_ROOT_LEN);
    path[NODE_ROOT_LEN] = '/';
    memcpy(path + NODE_ROOT_LEN + 1u, node, nlen + 1u);
    return path;
}

static void scan_buttons(joy_device_t *joydev, struct libevdev *evdev)
{
    uint32_t num = 0;

    if (libevdev_has_event_type(evdev, EV_KEY)) {
        size_t       size;
        unsigned int code;

        size = BUTTONS_INITIAL_SIZE;
        joydev->buttons = lib_malloc(size * sizeof *(joydev->buttons));

        for (code = BTN_MISC; code < KEY_MAX && num < UINT16_MAX; code++) {
            if (libevdev_has_event_code(evdev, EV_KEY, code)) {
                joy_button_t *button;

                if (num == size) {
                    size *= 2u;
                    joydev->buttons = lib_realloc(joydev->buttons,
                                                  size * sizeof *(joydev->buttons));
                }
                button = &(joydev->buttons[num++]);
                joy_button_init(button);
                button->code = (uint16_t)code;
                button->name = lib_strdup(get_button_name(code));
                //printf("%s\n", button->name);
            }
        }
    }
    joydev->num_buttons = num;
}



static void scan_axes(joy_device_t *joydev, struct libevdev *evdev)
{
    uint32_t num = 0;

    if (libevdev_has_event_type(evdev, EV_ABS)) {
        unsigned int code;
        size_t      size;

        size = AXES_INITIAL_SIZE;
        joydev->axes = lib_malloc(size * sizeof *(joydev->axes));

        for (code = ABS_X; code < ABS_RESERVED && num < UINT16_MAX; code++) {
            if (!IS_HAT(code) && libevdev_has_event_code(evdev, EV_ABS, code)) {
                const struct input_absinfo *absinfo;
                joy_axis_t                 *axis;

                if (num == size) {
                    size *= 2u;
                    joydev->axes = lib_realloc(joydev->axes,
                                               size * sizeof *(joydev->axes));
                }

                absinfo = libevdev_get_abs_info(evdev, code);
                axis    = &(joydev->axes[num]);
                joy_axis_init(axis);
                axis->code = (uint16_t)code;
                axis->name = lib_strdup(get_axis_name(code));
                //printf("axis name = %s\n", axis->name);
                if (absinfo != NULL) {
                    axis->minimum    = absinfo->minimum;
                    axis->maximum    = absinfo->maximum;
                    axis->fuzz       = absinfo->fuzz;
                    axis->flat       = absinfo->flat;
                    axis->resolution = absinfo->resolution;
                } else {
                    axis->minimum    = INT16_MIN;
                    axis->maximum    = INT16_MAX;
                }
                num++;
            }
        }
    }
    joydev->num_axes = num;
}

static void scan_hats(joy_device_t *joydev, struct libevdev *evdev)
{
    uint32_t num = 0;

    if (libevdev_has_event_type(evdev, EV_ABS)) {
        size_t h;

        /* only four hats defined in `input-event-codes.h`, so we simply allocate
         * space for four hats */
        joydev->hats = lib_malloc(HATS_INITIAL_SIZE * sizeof *(joydev->hats));

        for (h = 0; h < ARRAY_LEN(hat_event_codes); h++) {
            uint16_t x_code = hat_event_codes[h].x;
            uint16_t y_code = hat_event_codes[h].y;

            if (libevdev_has_event_code(evdev, EV_ABS, x_code) &&
                    libevdev_has_event_code(evdev, EV_ABS, y_code)) {

                const struct input_absinfo *absinfo;
                joy_hat_t                  *hat;
                joy_axis_t                 *x_axis;
                joy_axis_t                 *y_axis;

                hat    = &(joydev->hats[num]);
                x_axis = &(hat->x);
                y_axis = &(hat->y);
                joy_hat_init(hat);  /* also initializes the axes */

                /* X axis */
                absinfo = libevdev_get_abs_info(evdev, x_code);
                memset(x_axis, 0, sizeof *x_axis);
                x_axis->code = x_code;
                x_axis->name = lib_strdup(get_axis_name(x_code));
                if (absinfo != NULL) {
                    x_axis->minimum    = absinfo->minimum;
                    x_axis->maximum    = absinfo->maximum;
                    x_axis->fuzz       = absinfo->fuzz;
                    x_axis->flat       = absinfo->flat;
                    x_axis->resolution = absinfo->resolution;
                } else {
                    x_axis->minimum    = INT16_MIN;
                    x_axis->maximum    = INT16_MAX;
                }

                /* Y axis */
                absinfo = libevdev_get_abs_info(evdev, y_code);
                memset(y_axis, 0, sizeof *y_axis);
                y_axis->code = y_code;
                y_axis->name = lib_strdup(get_axis_name(y_code));
                if (absinfo != NULL) {
                    y_axis->minimum    = absinfo->minimum;
                    y_axis->maximum    = absinfo->maximum;
                    y_axis->fuzz       = absinfo->fuzz;
                    y_axis->flat       = absinfo->flat;
                    y_axis->resolution = absinfo->resolution;
                } else {
                    y_axis->minimum    = INT16_MIN;
                    y_axis->maximum    = INT16_MAX;
                }

                hat->name = lib_strdup(get_hat_name(x_code));
                //printf("hat name = %s\n", hat->name);

                num++;
            }
        }
    }

    joydev->num_hats = num;
}

static joy_device_t *get_device_data(const char *node)
{
    struct libevdev *evdev;
    joy_device_t    *joydev;
    int              fd;
    int              rc;

    fd = open(node, O_RDONLY|O_NONBLOCK);
    if (fd < 0) {
        /* Don't normally report error, a lot of nodes in dev/input aren't
         * readable by the user */
        msg_debug("Failed to open %s: %s -- ignoring\n", node, strerror(errno));
        return NULL;
    }

    msg_debug("Calling libevdev_new_from_fd(%d)\n", fd);
    rc = libevdev_new_from_fd(fd, &evdev);
    if (rc < 0) {
        fprintf(stderr, "%s(): failed to initialize libevdev: %s\n",
                __func__, strerror(-rc));
        close(fd);
        return NULL;
    }
    msg_debug("OK\n");

    joydev = joy_device_new();
    joydev->name    = lib_strdup(libevdev_get_name(evdev));
    joydev->node    = lib_strdup(node);
    joydev->vendor  = (uint16_t)libevdev_get_id_vendor(evdev);
    joydev->product = (uint16_t)libevdev_get_id_product(evdev);

    scan_buttons(joydev, evdev);
    scan_axes(joydev, evdev);
    scan_hats(joydev, evdev);

    joydev->priv = joydev_priv_new();

    libevdev_free(evdev);
    close(fd);
    return joydev;
}


int joy_device_list_init(joy_device_t ***devices)
{
    struct dirent **namelist = NULL;
    joy_device_t  **joylist;
    size_t          joylist_size;
    size_t          joylist_index;
    int             sr;     /* scandir result */

    sr = scandir(NODE_ROOT, &namelist, node_filter, NULL);
    if (sr < 0) {
        fprintf(stderr, "%s(): scandir failed on %s: %s.\n",
                __func__, NODE_ROOT, strerror(errno));
        return -1;
    }

    joylist_size  = DEVICES_INITIAL_SIZE;
    joylist_index = 0;
    joylist       = lib_malloc(joylist_size * sizeof *joylist);

    for (int i = 0; i < sr; i++) {
        joy_device_t *dev;
        char         *node;

        node = node_full_path(namelist[i]->d_name);
        dev  = get_device_data(node);
        if (dev != NULL) {
            /* -1 for the terminating NULL */
            if (joylist_index == joylist_size - 1u) {
                joylist_size *= 2u;
                joylist       = lib_realloc(joylist,
                                            joylist_size * sizeof *joylist);
            }
#if 0
            printf("node   : %s\n"
                   "name   : \"%s\"\n"
                   "vendor : %04"PRIx16"\n"
                   "product: %04"PRIx16"\n"
                   "buttons: %"PRIu32"\n"
                   "axes   : %"PRIu32"\n",
                dev->node,
                   dev->name,
                   dev->vendor,
                dev->product,
                   dev->num_buttons,
                   dev->num_axes);
#endif
            joylist[joylist_index++] = dev;
        }
        lib_free(node);
        free(namelist[i]);
    }
    free(namelist);

    joylist[joylist_index] = NULL;
    *devices               = joylist;
    return (int)joylist_index;
}



/** \brief  Driver \c open method
 *
 * Open the joystick device for polling.
 *
 * Associate a file descriptor and libevdev instance with the device through
 * its device node.
 *
 * \param[in]   joydev  joystick device
 *
 * \return  \c true on success
 */
static bool joydev_open(joy_device_t *joydev)
{
    joy_priv_t      *priv;
    struct libevdev *evdev;
    int              fd;
    int              rc;

    fd = open(joydev->node, O_RDONLY|O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "failed to open %s: %s\n", joydev->node, strerror(errno));
        return false;
    }

    rc = libevdev_new_from_fd(fd, &evdev);
    if (rc < 0) {
        fprintf(stderr, "failed to init libevdev: %s\n", strerror(-rc));
        close(fd);
        return false;
    }

    priv         = joydev->priv;
    priv->evdev  = evdev;
    priv->fd     = fd;
    return true;
}

/** \brief  Driver \c close method
 *
 * Close the device.
 *
 * Close the file descriptor and free the libdevdev instance.
 *
 * \param[in]   joydev  joystick device
 */
static void joydev_close(joy_device_t *joydev)
{
    joy_priv_t *priv = joydev->priv;

    if (priv != NULL) {
        if (priv->fd >= 0) {
            close(priv->fd);
            priv->fd = -1;
        }
        if (priv->evdev != NULL) {
            libevdev_free(priv->evdev);
            priv->evdev = NULL;
        }
    }
}


static int hat_to_joy_direction(int code, int value)
{
    int direction = 0;

    /* This currently assumes hats are reported with values of -1, 0 or 1,
     * which appears to be true for the controllers I have. */

    switch (code) {
        case ABS_HAT0X:
        case ABS_HAT1X:
        case ABS_HAT2X:
        case ABS_HAT3X:
            if (value < 0) {
                direction = JOYSTICK_DIRECTION_LEFT;
            } else if (value == 0) {
                direction = JOYSTICK_DIRECTION_NONE;
            } else {
                direction = JOYSTICK_DIRECTION_RIGHT;
            }
            break;
        case ABS_HAT0Y:
        case ABS_HAT1Y:
        case ABS_HAT2Y:
        case ABS_HAT3Y:
            if (value < 0) {
                direction = JOYSTICK_DIRECTION_UP;
            } else if (value == 0) {
                direction = JOYSTICK_DIRECTION_NONE;
            } else {
                direction = JOYSTICK_DIRECTION_DOWN;
            }
            break;
        default:
            break;
    }
    return direction;
}

static void poll_dispatch_event(joy_device_t *joydev, struct input_event *event)
{
    if (event->type == EV_SYN) {
        msg_verbose("event: time %ld.%06ld: %s\n",
                    event->input_event_sec,
                    event->input_event_usec,
                    libevdev_event_type_get_name(event->type));
    } else {
        if (verbose) {
            printf("event: time %ld.%06ld, type %d (%s), code %04x (%s), value %d\n",
                   event->input_event_sec,
                   event->input_event_usec,
                   event->type,
                   libevdev_event_type_get_name(event->type),
                   (unsigned int)event->code,
                   libevdev_event_code_get_name(event->type, event->code),
                   event->value);
        }

        if (IS_BUTTON(event->code)) {
            joy_button_event(joydev,
                             joy_button_from_code(joydev, event->code),
                             event->value);
        } else if (IS_AXIS(event->code)) {
            joy_axis_event(joydev,
                           joy_axis_from_code(joydev, event->code),
                           event->value);
        } else if (IS_HAT(event->code)) {
            joy_hat_event(joydev,
                          joy_hat_from_code(joydev, event->code),
                          hat_to_joy_direction(event->code, event->value));
        }
    }
}


static bool joydev_poll(joy_device_t *joydev)
{
    joy_priv_t         *priv;
    struct libevdev    *evdev;
    struct input_event  event;
    unsigned int        flags = LIBEVDEV_READ_FLAG_NORMAL;
    int                 fd;
    int                 rc;

    if (joydev == NULL || joydev->priv == NULL) {
        /* nothing to poll */
        return false;
    }

    priv  = joydev->priv;
    evdev = priv->evdev;
    fd    = priv->fd;
    if (evdev == NULL || fd < 0) {
        fprintf(stderr, "%s(): evdev is NULL or fd invalid\n", __func__);
        return false;
    }

    while (libevdev_has_event_pending(evdev)) {
        rc = libevdev_next_event(evdev, flags, &event);
        if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            if (verbose) {
                printf("=== dropped ===\n");
            }
            while (rc == LIBEVDEV_READ_STATUS_SYNC) {
                if (verbose) {
                    printf("sync");
                }
                rc = libevdev_next_event(evdev, LIBEVDEV_READ_FLAG_SYNC, &event);
            }
            if (verbose) {
                printf("=== resynced ===\n");
            }
        } else if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            poll_dispatch_event(joydev, &event);
        }
    }

    return true;
}


bool joy_init(void)
{
    joy_driver_t driver = {
        .open      = joydev_open,
        .close     = joydev_close,
        .poll      = joydev_poll,
        .priv_free = joydev_priv_free
    };

    joy_driver_register(&driver);
    return true;
}
