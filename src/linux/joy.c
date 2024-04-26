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

/** \brief  Hardware-specific data
 *
 * Allocated during device detection, used in the \c open(), \c poll() and
 * \c close() driver callbacks, freed via the driver's \c hwdata_free() callback.
 */
typedef struct hwdata_s {
    struct libevdev *evdev;     /**< evdev instance */
    int              fd;        /**< file descriptor */
} hwdata_t;


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

#define IS_HAT_X_AXIS(code) ((code) == ABS_HAT0X || (code) == ABS_HAT1X || \
                             (code) == ABS_HAT2X || (code) == ABS_HAT3X)

#define IS_HAT_Y_AXIS(code) ((code) == ABS_HAT0Y || (code) == ABS_HAT1Y || \
                             (code) == ABS_HAT2Y || (code) == ABS_HAT3Y)


/** \brief X and Y axes for the four hats found in `linux/input-event-codes.h` */
static const hat_evcode_t hat_event_codes[] = {
    { ABS_HAT0X, ABS_HAT0Y },
    { ABS_HAT1X, ABS_HAT1Y },
    { ABS_HAT2X, ABS_HAT2Y },
    { ABS_HAT3X, ABS_HAT3Y }
};


static hwdata_t *hwdata_new(void)
{
    hwdata_t *hwdata= lib_malloc(sizeof *hwdata);

    hwdata->evdev = NULL;
    hwdata->fd    = -1;
    return hwdata;
}

static void hwdata_free(void *hwdata)
{
    hwdata_t *hw = hwdata;

    if (hw != NULL) {
        if (hw->evdev) {
            libevdev_free(hw->evdev);
        }
        if (hw->fd >= 0) {
            close(hw->fd);
        }
    }
    lib_free(hwdata);
}

static const char *get_hat_name(unsigned int code)
{
    switch (code) {
        case ABS_HAT0X: /* fall through */
        case ABS_HAT0Y:
            return "ABS_HAT0";
        case ABS_HAT1X: /* fall through */
        case ABS_HAT1Y:
            return "ABS_HAT1";
        case ABS_HAT2X: /* fall through */
        case ABS_HAT2Y:
            return "ABS_HAT2";
        case ABS_HAT3X: /* fall through */
        case ABS_HAT3Y:
            return "ABS_HAT3";
        default:
            return NULL;
    }
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
                button->name = lib_strdup(libevdev_event_code_get_name(EV_KEY, code));
                //printf("%s\n", button->name);
            }
        }
    }
    joydev->num_buttons = num;
}


static bool axis_is_digital(joy_axis_t *axis)
{
    if (axis->minimum == -1 && axis->maximum == 1) {
        /* definitely digital */
        return true;
        axis->digital = true;
    } else if (axis->flat == 0 && axis->fuzz == 0 && axis->resolution == 0) {
        /* most likely digital */
        /* XXX: SDL uses this logic, but it doesn't work correctly
         *      for the analog triggers ABS_Z and ABS_RZ on my
         *      Logitech F710, so it's commented out for now.
         */
#if 0
        return true;
#endif
    }
    return false;
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
                axis->name = lib_strdup(libevdev_event_code_get_name(EV_ABS, code));
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

                /* determine if we're dealing with a digital or an analog axis */
                axis->digital = axis_is_digital(axis);

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
                x_axis->code = x_code;
                x_axis->name = lib_strdup(libevdev_event_code_get_name(EV_ABS, x_code));
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
                x_axis->digital = axis_is_digital(x_axis);

                /* Y axis */
                absinfo = libevdev_get_abs_info(evdev, y_code);
                y_axis->code = y_code;
                y_axis->name = lib_strdup(libevdev_event_code_get_name(EV_ABS, y_code));
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
                y_axis->digital = axis_is_digital(y_axis);

                hat->name = lib_strdup(get_hat_name(x_code));
                hat->code = x_code;
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
    joydev->version = (uint16_t)libevdev_get_id_version(evdev);

    scan_buttons(joydev, evdev);
    scan_axes(joydev, evdev);
    scan_hats(joydev, evdev);

    joydev->hwdata = hwdata_new();

    libevdev_free(evdev);
    close(fd);
    return joydev;
}


int joy_arch_device_list_init(joy_device_t ***devices)
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
            /* determine capabilities for emulated devices */
            joy_device_set_capabilities(dev);
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
    hwdata_t       * hwdata;
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

    hwdata        = joydev->hwdata;
    hwdata->evdev = evdev;
    hwdata->fd    = fd;
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
    hwdata_t *hwdata = joydev->hwdata;

    if (hwdata != NULL) {
        if (hwdata->fd >= 0) {
            close(hwdata->fd);
            hwdata->fd = -1;
        }
        if (hwdata->evdev != NULL) {
            libevdev_free(hwdata->evdev);
            hwdata->evdev = NULL;
        }
    }
}

#if 0
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
#endif

static uint16_t get_hat_x_for_hat_code(uint16_t code)
{
    switch (code) {
        case ABS_HAT0X:
        case ABS_HAT0Y:
            return ABS_HAT0X;
        case ABS_HAT1X:
        case ABS_HAT1Y:
            return ABS_HAT1X;
        case ABS_HAT2X:
        case ABS_HAT2Y:
            return ABS_HAT2X;
        case ABS_HAT3X:
        case ABS_HAT3Y:
            return ABS_HAT3X;
        default:
            return 0;
    }
}

static void poll_dispatch_event(joy_device_t *joydev, struct input_event *event)
{
    joy_hat_t             *hat;
    joy_axis_t            *axis;
    joystick_axis_value_t  axis_value;

    if (event->type == EV_SYN) {
        msg_verbose("event: time %ld.%06ld: %s\n",
                    event->input_event_sec,
                    event->input_event_usec,
                    libevdev_event_type_get_name(event->type));
    } else {
        msg_verbose("event: time %ld.%06ld, type %d (%s), code %04x (%s), value %d\n",
                    event->input_event_sec,
                    event->input_event_usec,
                    event->type,
                    libevdev_event_type_get_name(event->type),
                    (unsigned int)event->code,
                    libevdev_event_code_get_name(event->type, event->code),
                    event->value);

        if (event->type == EV_KEY && IS_BUTTON(event->code)) {
            joy_button_event(joydev,
                             joy_button_from_code(joydev, event->code),
                             event->value);
        } else if (event->type == EV_ABS && IS_AXIS(event->code)) {
            /* TODO: configurable threshold/deadzone */
            axis       = joy_axis_from_code(joydev, event->code);
            axis_value = joy_axis_value_from_hwdata(axis, event->value);
            joy_axis_event(joydev, axis, axis_value);

        } else if (event->type == EV_ABS && IS_HAT(event->code)) {
            joy_mapping_t *mapping;
            int32_t        x_pins = 0;
            int32_t        y_pins = 0;

            hat = joy_hat_from_code(joydev, get_hat_x_for_hat_code(event->code));
            if (hat == NULL) {
                msg_error("hat is NULL\n");
                return;
            }

            /* On Linux we don't have hats as a single object but rather two
             * axes combined into a "virtual" hat. The values for this "hat"
             * come in with two separate event codes, one for each axis, so
             * we combine the current axis of a hat with the previous value of
             * the hat's other axis to determine the actual hat value: */
            if (IS_HAT_X_AXIS(event->code)) {
                axis_value = joy_axis_value_from_hwdata(&(hat->x), event->value);
                if (axis_value == JOY_AXIS_NEGATIVE) {
                    mapping = &(hat->mapping.left);
                    x_pins = JOYSTICK_DIRECTION_LEFT;
                } else if (axis_value == JOY_AXIS_POSITIVE) {
                    mapping = &(hat->mapping.right);
                    x_pins = JOYSTICK_DIRECTION_RIGHT;
                }
                hat->x.prev = x_pins;
                y_pins      = hat->y.prev;
            } else {
                axis_value = joy_axis_value_from_hwdata(&(hat->y), event->value);
                if (axis_value == JOY_AXIS_NEGATIVE) {
                    mapping = &(hat->mapping.up);
                    y_pins = JOYSTICK_DIRECTION_UP;
                } else if (axis_value == JOY_AXIS_POSITIVE) {
                    mapping = &(hat->mapping.down);
                    y_pins = JOYSTICK_DIRECTION_DOWN;
                }
                hat->y.prev = y_pins;
                x_pins      = hat->x.prev;
            }

            joy_hat_event(joydev, hat, mapping, x_pins|y_pins);
        }
    }
}


static bool joydev_poll(joy_device_t *joydev)
{
    hwdata_t           *hwdata;
    struct libevdev    *evdev;
    struct input_event  event;
    unsigned int        flags = LIBEVDEV_READ_FLAG_NORMAL;
    int                 fd;
    int                 rc;

    if (joydev == NULL || joydev->hwdata == NULL) {
        /* nothing to poll */
        return false;
    }

    hwdata = joydev->hwdata;
    evdev  = hwdata->evdev;
    fd     = hwdata->fd;
    if (evdev == NULL || fd < 0) {
        fprintf(stderr, "%s(): evdev is NULL or fd invalid\n", __func__);
        return false;
    }

    while (libevdev_has_event_pending(evdev)) {
        rc = libevdev_next_event(evdev, flags, &event);
        if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            msg_debug("=== DROPPED ===\n");
            while (rc == LIBEVDEV_READ_STATUS_SYNC) {
                msg_debug("=== SYNCING ===\n");
                rc = libevdev_next_event(evdev, LIBEVDEV_READ_FLAG_SYNC, &event);
            }
            msg_debug("=== RESYNCED ===\n");
        } else if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            if (event.type == EV_ABS || event.type == EV_KEY) {
                poll_dispatch_event(joydev, &event);
            }
        }
    }

    return true;
}


bool joy_arch_init(void)
{
    joy_driver_t driver = {
        .open        = joydev_open,
        .close       = joydev_close,
        .poll        = joydev_poll,
        .hwdata_free = hwdata_free
    };

    joy_driver_register(&driver);
    return true;
}


bool joy_arch_device_create_default_mapping(joy_device_t *joydev)
{
    joy_mapping_t *mapping;
    joy_axis_t    *axis;
    joy_button_t  *button;
    joy_hat_t     *hat;

    if (joydev->capabilities == JOY_CAPS_NONE) {
        printf("%s(): no capabilites for device %s\n", __func__, joydev->name);
        return false;
    }

    /* first try joystick */
    if (joydev->capabilities & JOY_CAPS_JOYSTICK) {
        if (joydev->num_hats >= 1u) {
            printf("%s(): got at least one hat\n", __func__);
            /* map (first) hat to pins */
            hat = &(joydev->hats[0]);

#if 0
            /* negative direction of X axis */
            hat->x.mapping.negative.action     = JOY_ACTION_JOYSTICK;
            hat->x.mapping.negative.target.pin = JOYSTICK_DIRECTION_LEFT;

            /* positive direction of X axis */
            hat->x.mapping.positive.action     = JOY_ACTION_JOYSTICK;
            hat->x.mapping.positive.target.pin = JOYSTICK_DIRECTION_RIGHT;

            /* negative direction of Y axis */
            hat->y.mapping.negative.action     = JOY_ACTION_JOYSTICK;
            hat->y.mapping.negative.target.pin = JOYSTICK_DIRECTION_UP;
            /* positive direction of Y axis */
            hat->y.mapping.positive.action     = JOY_ACTION_JOYSTICK;
            hat->y.mapping.positive.target.pin = JOYSTICK_DIRECTION_DOWN;
#endif
            hat->mapping.up.action        = JOY_ACTION_JOYSTICK;
            hat->mapping.up.target.pin    = JOYSTICK_DIRECTION_UP;
            hat->mapping.down.action      = JOY_ACTION_JOYSTICK;
            hat->mapping.down.target.pin  = JOYSTICK_DIRECTION_DOWN;
            hat->mapping.left.action      = JOY_ACTION_JOYSTICK;
            hat->mapping.left.target.pin  = JOYSTICK_DIRECTION_LEFT;
            hat->mapping.right.action     = JOY_ACTION_JOYSTICK;
            hat->mapping.right.target.pin = JOYSTICK_DIRECTION_RIGHT;

        } else if (joydev->num_axes >= 2u) {
            /* assume first axis to be X axis */
            axis = &(joydev->axes[0]);
            /* negative -> left */
            axis->mapping.negative.action     = JOY_ACTION_JOYSTICK;
            axis->mapping.positive.target.pin = JOYSTICK_DIRECTION_LEFT;
            /* positive -> right */
            axis->mapping.positive.action     = JOY_ACTION_JOYSTICK;
            axis->mapping.positive.target.pin = JOYSTICK_DIRECTION_RIGHT;

            /* second axis: X axis */
            axis = &(joydev->axes[1]);
            /* negative -> up */
            axis->mapping.negative.action     = JOY_ACTION_JOYSTICK;
            axis->mapping.negative.target.pin = JOYSTICK_DIRECTION_UP;
            /* positive -> down */
            axis->mapping.positive.action     = JOY_ACTION_JOYSTICK;
            axis->mapping.positive.target.pin = JOYSTICK_DIRECTION_DOWN;
        }

        button  = &(joydev->buttons[0]);
        mapping = &(button->mapping);
        mapping->action     = JOY_ACTION_JOYSTICK;
        mapping->target.pin = 16;
    }

    return true;
}
