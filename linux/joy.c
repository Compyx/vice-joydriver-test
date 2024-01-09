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

#include <dirent.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "joyapi.h"
#include "lib.h"


#define DEVICES_INITIAL_SIZE    16
#define BUTTONS_INITIAL_SIZE    32
#define AXES_INITIAL_SIZE       16
#define HATS_INITIAL_SIZE       4

#define NODE_ROOT               "/dev/input"
#define NODE_ROOT_LEN           10

#define NODE_PREFIX             "event"
#define NODE_PREFIX_LEN         5


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
    size_t num = 0;

    if (libevdev_has_event_type(evdev, EV_KEY)) {
        size_t       size;
        unsigned int code;

        size = BUTTONS_INITIAL_SIZE;
        joydev->buttons = lib_malloc(size * sizeof *(joydev->buttons));

        for (code = BTN_MISC; code < KEY_MAX && num < UINT16_MAX; code++) {
            if (libevdev_has_event_code(evdev, EV_KEY, code)) {
                if (num == size) {
                    size *= 2u;
                    joydev->buttons = lib_realloc(joydev->buttons,
                                                  size * sizeof *(joydev->buttons));
                }
                joydev->buttons[num++] = (uint16_t)code;
            }
        }
    }
    joydev->num_buttons = (uint16_t)num;
}

/** \brief  Test if an event code is a hat axis code
 *
 * \param[in]   code    event code
 * \return  \c true if \a code is a hat event
 */
#define IS_HAT(code) ((code) >= ABS_HAT0X && (code) <= ABS_HAT3Y)


static void scan_axes(joy_device_t *joydev, struct libevdev *evdev)
{
    size_t num = 0;

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
                memset(axis, 0, sizeof *axis);
                axis->code = (uint16_t)code;
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
    joydev->num_axes = (uint16_t)num;
}

static joy_device_t *get_device_data(const char *node)
{
    struct libevdev *evdev;
    joy_device_t    *joydev;
    int              fd;
    int              rc;

    fd = open(node, O_RDONLY|O_NONBLOCK);
    if (fd < 0) {
        /* can't report error, a lot of nodes in dev/input aren't readable
         * by the user */
#if 0
        fprintf(stderr, "%s(): failed to open %s: %s\n",
                __func__, node, strerror(errno));
#endif
        return NULL;
    }

    rc = libevdev_new_from_fd(fd, &evdev);
    if (rc < 0) {
        fprintf(stderr, "%s(): failed to initialize libevdev: %s\n",
                __func__, strerror(-rc));
        close(fd);
        return NULL;
    }

    joydev = joy_device_new();
    joydev->name    = lib_strdup(libevdev_get_name(evdev));
    joydev->node    = lib_strdup(node);
    joydev->vendor  = (uint16_t)libevdev_get_id_vendor(evdev);
    joydev->product = (uint16_t)libevdev_get_id_product(evdev);

    scan_buttons(joydev, evdev);
    scan_axes(joydev, evdev);

    libevdev_free(evdev);
    close(fd);
    return joydev;
}


int joy_get_devices(joy_device_t ***devices)
{
    struct dirent **namelist = NULL;
    joy_device_t  **list;
    size_t          size;
    size_t          num;    /* number of joystick devices found */
    int             sr;     /* scandir result */

    sr = scandir(NODE_ROOT, &namelist, node_filter, NULL);
    if (sr < 0) {
        fprintf(stderr, "%s(): scandir failed on %s: %s.\n",
                __func__, NODE_ROOT, strerror(errno));
        return -1;
    }

    size = DEVICES_INITIAL_SIZE;
    list = lib_malloc(size * sizeof *list);
    num  = 0;

    for (int i = 0; i < sr; i++) {
        joy_device_t *dev;
        char         *node;

        node = node_full_path(namelist[i]->d_name);
        dev  = get_device_data(node);
        if (dev != NULL) {

            if (num == size - 1u) {
                size *= 2u;
                list  = lib_realloc(list, size * sizeof *list);
            }

            printf("node   : %s\n"
                   "name   : \"%s\"\n"
                   "vendor : %04x\n"
                   "product: %04x\n"
                   "buttons: %u\n"
                   "axes   : %u\n",
                   dev->node, dev->name,
                   (unsigned int)dev->vendor, (unsigned int)dev->product,
                   (unsigned int)dev->num_buttons, (unsigned int)dev->num_axes);

            list[num++] = dev;
        }
        lib_free(node);
        free(namelist[i]);
    }

    free(namelist);
    list[num] = NULL;
    *devices = list;
    return (int)num;
}


void joy_free_devices(joy_device_t **devices)
{
    if (devices != NULL) {
        for (size_t i = 0; devices[i] != NULL; i++) {
            joy_device_free(devices[i]);
        }
        lib_free(devices);
    }
}
