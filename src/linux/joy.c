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


/* defined in main.c, set by --debug and --verbose */
extern bool debug;
extern bool verbose;


/** \brief  Initial size of devices array */
#define DEVICES_INITIAL_SIZE    16

/** \brief  Initial size of buttons array */
#define BUTTONS_INITIAL_SIZE    32

/** \brief  Initial size of axes array */
#define AXES_INITIAL_SIZE       16

/** \brief  Root directory for the event interface devices */
#define NODE_ROOT               "/dev/input"

/** \brief  Length of #NODE_ROOT */
#define NODE_ROOT_LEN           10

/** \brief  Prefix of event devices */
#define NODE_PREFIX             "event"

/** \brief  Length of #NODE_PREFIX */
#define NODE_PREFIX_LEN         5


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
#define IS_AXIS(code) ((code) >= ABS_X && (code) < ABS_MISC)

/** \brief  Test if an event code is a button event code
 *
 * \param[in]   code    event code
 * \return  \c true if button
 */
#define IS_BUTTON(code) ((code) >= BTN_JOYSTICK && (code) <= BTN_THUMBR)


/** \brief  Allocate and initialize driver-specific data object
 *
 * \return  new driver-specific data object
 */
static hwdata_t *hwdata_new(void)
{
    hwdata_t *hwdata= lib_malloc(sizeof *hwdata);

    hwdata->evdev = NULL;
    hwdata->fd    = -1;
    return hwdata;
}

/** \brief  Free driver-specific data
 *
 * Also close any open file descriptor and libevdev instance.
 *
 * \param[in]   hwdata  driver-specific data
 */
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

/** \brief  Filter callback for scandir()
 *
 * \param[in]   de  dirent
 *
 * \return  \c 1 if the filename matches "event?*", \c 0 otherwise
 */
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

/** \brief  Generate full path from node in /dev/input/
 *
 * Concatenate "/dev/input/" and \a node and return heap allocated string.
 *
 * \return  full path to input nod
 *
 * \note    free result after use with \c lib_malloc()
 */
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

/** \brief  Scan buttons of evdev device and store in joydev
 *
 * Scan device for valid button codes. Unlike other APIs like DirectInput, SDL
 * or BSD's usbhid, the event interface buttons aren't numbered sequentially
 * starting from 0, but have predefined event codes (these are defined in
 * \c /usr/include/linux/input-event-codes). So to build a list of buttons we
 * iterate a range of event codes (\c BTN_MISC (0x100) to \c KEY_MAX (0x2ff)
 * and check if the device has an event for these codes.
 *
 * \param[in]   joydev  joystick device
 * \param[in]   evdev   libevdev instance
 */
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

/** \brief  Determine if an axis is digital
 *
 * Determine if \a axis is digital, meaning it reports only three different
 * values: negative, centered and positive.
 *
 * \param[in]   axis
 *
 * \return  \c true if digital
 */
static bool axis_is_digital(joy_axis_t *axis)
{
    if (axis->minimum == -1 && axis->maximum == 1) {
        /* definitely digital */
        return true;
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

/** \brief  Scan axes of evdev device and store in joydev
 *
 * Iterate event codes for axes and store information on absolute axes supported
 * by the device.
 *
 * \param[in]   joydev  joystick device
 * \param[in]   evdev   libevdev instance
 */
static void scan_axes(joy_device_t *joydev, struct libevdev *evdev)
{
    uint32_t num = 0;

    if (libevdev_has_event_type(evdev, EV_ABS)) {
        unsigned int code;
        size_t      size;

        size = AXES_INITIAL_SIZE;
        joydev->axes = lib_malloc(size * sizeof *(joydev->axes));

        for (code = ABS_X; code < ABS_RESERVED && num < UINT16_MAX; code++) {
            if (libevdev_has_event_code(evdev, EV_ABS, code)) {
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
                /* auto-calibrate */
                joy_axis_auto_calibrate(axis);

                num++;
            }
        }
    }
    joydev->num_axes = num;
}

/** \brief  Try to obtain information on a possible input device
 *
 * Try to open \c node and obtain a device's name, vendor ID, product ID and
 * its supported input events.
 *
 * \param[in]   node    path in the file system of an input device
 *
 * \return  new joystick device or \c NULL on failure
 */
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
    joydev->num_hats = 0;

    joydev->hwdata = hwdata_new();

    libevdev_free(evdev);
    close(fd);
    return joydev;
}


/** \brief  Create list of input devices
 *
 * \param[out]  devices list of supported input devices
 *
 * \return  number of devices in \a devices or -1 on error
 */
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

/** \brief  Dispatch input event to generic VICE joystick code
 *
 * \param[in]   joydev  joystick device
 * \param[in]   event   input event
 */
static void poll_dispatch_event(joy_device_t *joydev, struct input_event *event)
{
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
        }
    }
}

/** \brief  Driver poll method
 *
 * Poll \a joydev for pending events and pass them to the generic joystick
 * code.
 *
 * \param[in]   joydev  joystick device
 *
 * \return  \c true to keep polling, \a false on fatal error
 */
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


/** \brief  Initialize driver-specific data
 *
 * Register driver with VICE.
 *
 * \return  \c true on success (currently always returns \c true)
 */
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


/** \brief  Clean up driver-specific data
 */
void joy_arch_shutdown(void)
{
    /* NOP for now */
}


/** \brief  Create default mapping for a joystick device
 *
 * Create mapping of four joystick directions and one fire button.
 *
 * \param[in]   joydev  joystick device
 *
 * \return  \c true on success
 *
 * \note    currently always returns \c true, perhaps this function could
 *          return \c false if \a joydev doesn't have the required inputs
 *          for a valid mapping?
 */
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
