/** \file   joy.c
 * \brief   BSD joystick driver
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>

//#include <dev/usb/usb.h>
//#include <dev/usb/usbdi.h>
//#include <dev/usb/usbdi_util.h>
//#include <dev/usb/usbhid.h>
#include <usbhid.h>

#ifdef FREEBSD_COMPILE
/* for hid_* and HUG_* */
#include <dev/hid/hid.h>
/* for struct usb_device_info */
#include <dev/usb/usb_ioctl.h>
#endif

#ifdef NETBSD_COMPILE
#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>
#include <dev/hid/hid.h>

/* FreeBSD (9.3) doesn't have the D-Pad defines */
#ifndef HUG_D_PAD_UP
#define HUG_D_PAD_UP    0x0090
#endif
#ifndef HUG_D_PAD_DOWN
#define HUG_D_PAD_DOWN  0x0091
#endif
#ifndef HUG_D_PAD_RIGHT
#define HUG_D_PAD_RIGHT 0x0092
#endif
#ifndef HUG_D_PAD_LEFT
#define HUG_D_PAD_LEFT  0x0093
#endif

#endif

#include "lib.h"
#include "joyapi.h"


#define ROOT_NODE       "/dev"
#define ROOT_NODE_LEN   strlen(ROOT_NODE)
#define NODE_PREFIX     "uhid"
#define NODE_PREFIX_LEN strlen(NODE_PREFIX)

typedef struct dpad_pin_s {
    uint16_t code;
    int      pin;
} dpad_pin_t;

typedef struct joy_hwdata_s {
    void          *buffer;
    report_desc_t  rep_desc;
    ssize_t        rep_size;
    int            rep_id;
    int            fd;
} joy_hwdata_t;


static const dpad_pin_t dpad_pins[4] = {
    { HUG_D_PAD_UP,    JOYSTICK_DIRECTION_UP },
    { HUG_D_PAD_DOWN,  JOYSTICK_DIRECTION_DOWN },
    { HUG_D_PAD_LEFT,  JOYSTICK_DIRECTION_LEFT },
    { HUG_D_PAD_RIGHT, JOYSTICK_DIRECTION_RIGHT }
};

static joy_hwdata_t *joy_hwdata_new(void)
{
    joy_hwdata_t *hwdata = lib_malloc(sizeof *hwdata);

    hwdata->fd       = -1;
    hwdata->rep_id   = 0;
    hwdata->rep_size = 0;
    hwdata->buffer   = NULL;
    hwdata->rep_desc = NULL;

    return hwdata;
}

static void joy_hwdata_free(void *hwdata)
{
    joy_hwdata_t *hw = hwdata;
    if (hw != NULL) {
        if (hw->fd >= 0) {
            close(hw->fd);
        }
        if (hw->buffer != NULL) {
            lib_free(hw->buffer);
        }
        if (hw->rep_desc != NULL) {
            hid_dispose_report_desc(hw->rep_desc);
        }
        lib_free(hw);
    }
}

static bool has_dpad(joy_device_t *joydev)
{
    if (joydev->num_buttons >= 4u) {
        uint32_t dpad = 0;
        uint32_t b;
        uint32_t d;

        for (b = 0; b < joydev->num_buttons; b++) {
            for (d = 0; d < ARRAY_LEN(dpad_pins); d++) {
                if (joydev->buttons[b].code == dpad_pins[b].code) {
                    dpad |= (1u << b);
                    break;
                }
            }
        }
        if (dpad == 0x0f) {
            return true;
        }
    }
    return false;
}

static int sd_select(const struct dirent *de)
{
    const char *name = de->d_name;

    return ((strlen(name) >= NODE_PREFIX_LEN + 1u) &&
            (strncmp(NODE_PREFIX, name, NODE_PREFIX_LEN) == 0));
}

static char *get_full_node_path(const char *node)
{
    size_t  nlen = strlen(node);
    size_t  plen = ROOT_NODE_LEN + 1u + nlen + 1u;
    char   *path = lib_malloc(plen);

    memcpy(path, ROOT_NODE, ROOT_NODE_LEN);
    path[ROOT_NODE_LEN] = '/';
    memcpy(path + ROOT_NODE_LEN + 1, node, nlen + 1u);
    return path;
}

/** \brief  Iterator for scanning buttons */
typedef struct {
    joy_button_t *list;     /**< list of buttons */
    size_t        size;     /**< number of elements allocated in \c list */
    size_t        index;    /**< index in \c list */
} button_iter_t;

typedef struct {
    joy_axis_t  *list;
    size_t       size;
    size_t       index;
} axis_iter_t;

typedef struct {
    joy_hat_t   *list;
    size_t       size;
    size_t       index;
} hat_iter_t;

/** \brief  Initialize device inputs list iterator
 *
 * \param[in]   iter    pointer to iterator
 * \param[in]   _size   number of elements to allocate in \c list
 */
#define LIST_ITER_INIT(iter, _size) \
    (iter)->size   = _size; \
    (iter)->index  = 0; \
    (iter)->list   = lib_malloc(_size * sizeof *((iter)->list));

/** \brief  Resize list of device inputs iterator
 *
 * Check if there still is space for an element in the \a iter's \c list, and
 * if not double the size of the \c list.
 *
 * \param[in]   iter    pointer to iterator
 */
#define LIST_ITER_RESIZE(iter) \
    if (iter->index == iter->size) { \
        iter->size *= 2u; \
        iter->list  = lib_realloc(iter->list, iter->size * sizeof *(iter->list)); \
    }

static void add_joy_button(button_iter_t *iter, const struct hid_item *item)
{
    joy_button_t *button;

    LIST_ITER_RESIZE(iter);

    button = &(iter->list[iter->index++]);
    joy_button_init(button);
    button->code = (uint16_t)HID_USAGE(item->usage);
    button->name = lib_strdup(hid_usage_in_page(item->usage));
    msg_debug("adding button %u: %s\n",
              (unsigned int)button->code, button->name);
}

static void add_joy_axis(axis_iter_t *iter, const struct hid_item *item)
{
    joy_axis_t *axis;

    LIST_ITER_RESIZE(iter);

    axis = &(iter->list[iter->index++]);
    joy_axis_init(axis);
    axis->code = (uint16_t)HID_USAGE(item->usage);
    axis->name = lib_strdup(hid_usage_in_page(item->usage));
    axis->minimum = item->logical_minimum;
    axis->maximum = item->logical_maximum;
    msg_debug("adding axis %u: %s (%d - %d)\n",
             (unsigned int)axis->code, axis->name, axis->minimum, axis->maximum);
}

static void add_joy_hat(hat_iter_t *iter, const struct hid_item *item)
{
    joy_hat_t *hat;

    LIST_ITER_RESIZE(iter);
    hat = &(iter->list[iter->index++]);
    joy_hat_init(hat);
    hat->name = lib_strdup(hid_usage_in_page(item->usage));
    msg_debug("adding hat %u: %s\n",
              HID_USAGE(item->usage), hat->name);
}


static joy_device_t *get_device_data(const char *node)
{
    joy_device_t           *joydev;
    joy_hwdata_t           *hwdata;
    struct usb_device_info  devinfo;
    report_desc_t           report;
    int                     rep_id;
    int                     rep_size;
    struct hid_data        *hdata;
    struct hid_item         hitem;
    char                   *name;
    int                     fd;
    axis_iter_t             axis_iter;
    button_iter_t           button_iter;
    hat_iter_t              hat_iter;

    fd = open(node, O_RDONLY|O_NONBLOCK);
    if (fd < 0) {
        // fprintf(stderr, "%s(): failed to open %s.\n", __func__, node);
        return NULL;
    }

    /* get device info for vendor and product */
    if (ioctl(fd, USB_GET_DEVICEINFO, &devinfo) < 0) {
        /* fprintf(stderr, "%s(): ioctl failed: %s.\n",
                __func__, strerror(errno)); */
        close(fd);
        return NULL;
    }

    /* get report ID if possible, otherwise assume 0 */
#ifdef USE_GET_REPORT_ID
    if (ioctl(fd, USB_GET_REPORT_ID, &rep_id) < 0) {
        fprintf(stderr, "%s(): could not get USB report id: %s.\n",
                __func__, strerror(errno));
        close(fd);
        return NULL;
    }
#else
    rep_id = 0;
#endif

    report = hid_get_report_desc(fd);
    if (report == NULL) {
        if (debug) {
            fprintf(stderr, "%s(): failed to get HID report for %s: %d: %s.\n",
                    __func__, node, errno, strerror(errno));
        }
        close(fd);
        return NULL;
    }

    rep_size = hid_report_size(report, hid_input, rep_id);
    //printf("%s(): report size = %d\n", __func__, rep_size);
    if (rep_size <= 0) {
        if (debug) {
            fprintf(stderr,
                    "%s(): error: invalid report size of %d.\n",
                    __func__, rep_size);
        }
        hid_dispose_report_desc(report);
        close(fd);
        return NULL;
    }

    /* construct device name */
    name = util_concat(devinfo.udi_vendor, " ", devinfo.udi_product, NULL);
    if (*name == '\0') {
        /* fall back to device node */
        lib_free(name);
        name = lib_strdup(node);
    }

    joydev          = joy_device_new();
    joydev->node    = lib_strdup(node);
    joydev->name    = name;
    joydev->vendor  = devinfo.udi_vendorNo;
    joydev->product = devinfo.udi_productNo;
    joydev->version = devinfo.udi_releaseNo;

    LIST_ITER_INIT(&axis_iter, 8u);
    LIST_ITER_INIT(&button_iter, 16u);
    LIST_ITER_INIT(&hat_iter, 4u);

    hdata = hid_start_parse(report, 1 << hid_input, rep_id);
    if (hdata == NULL) {
        if (debug) {
            fprintf(stderr, "%s(): hid_start_parse() failed: %d (%s).\n",
                    __func__, errno, strerror(errno));
        }
        hid_dispose_report_desc(report);
        close(fd);
        joy_device_free(joydev);
        return NULL;
    }
    while (hid_get_item(hdata, &hitem) > 0) {
        unsigned int page  = HID_PAGE (hitem.usage);
        int          usage = HID_USAGE(hitem.usage);

        switch (page) {
            case HUP_GENERIC_DESKTOP:
                switch (usage) {
                    case HUG_X:     /* fall through */
                    case HUG_Y:     /* fall through */
                    case HUG_Z:     /* fall through */
                    case HUG_RX:    /* fall through */
                    case HUG_RY:    /* fall through */
                    case HUG_RZ:    /* fall through */
                    case HUG_SLIDER:
                        /* got an axis */
                        add_joy_axis(&axis_iter, &hitem);
                        break;
                    case HUG_HAT_SWITCH:
                        /* hat, seems to be D-Pad on Logitech F710 */
                        add_joy_hat(&hat_iter, &hitem);
                        break;
                    case HUG_D_PAD_UP:      /* fall through */
                    case HUG_D_PAD_DOWN:    /* fall through */
                    case HUG_D_PAD_LEFT:    /* fall through */
                    case HUG_D_PAD_RIGHT:
                        /* treat D-Pad as buttons */
                        add_joy_button(&button_iter, &hitem);
                        break;
                    default:
                        break;
                }
                break;
            case HUP_BUTTON:
                /* usage appears to be the button number */
                add_joy_button(&button_iter, &hitem);
                break;
            default:
                break;
        }
    }
    hid_end_parse(hdata);

    joydev->axes         = axis_iter.list;
    joydev->num_axes     = (uint32_t)axis_iter.index;
    joydev->buttons      = button_iter.list;
    joydev->num_buttons  = (uint32_t)button_iter.index;
    joydev->hats         = hat_iter.list;
    joydev->num_hats     = (uint32_t)hat_iter.index;

    hwdata           = joy_hwdata_new();
    hwdata->fd       = -1;
    hwdata->rep_id   = rep_id;
    hwdata->rep_size = rep_size;
    hwdata->buffer   = lib_malloc((size_t)rep_size);
    hwdata->rep_desc = report;

    joydev->hwdata = hwdata;

    close(fd);
    return joydev;
}


int joy_arch_device_list_init(joy_device_t ***devices)
{
    joy_device_t  **joylist;
    struct dirent **namelist = NULL;
    int             nl_count = 0;
    int             n;
    size_t          joylist_size;
    size_t          joylist_index;

    if (devices != NULL) {
        *devices = NULL;
    }

    nl_count = scandir(ROOT_NODE, &namelist, sd_select, NULL);
    if (nl_count < 0) {
        fprintf(stderr, "%s(): scandir failed: %s.\n", __func__, strerror(errno));
    } else if (nl_count == 0) {
        return 0;   /* no devices found */
    }

    joylist_size  = 8u;
    joylist_index = 0;
    joylist       = lib_malloc(joylist_size * sizeof *joylist);
    joylist[0]    = NULL;

    /* initialize HID library so we can retrieve strings for page and usage */
    hid_init(NULL);

    for (n = 0 ; n < nl_count; n++) {
        joy_device_t *joydev;
        char         *node;

        node         = get_full_node_path(namelist[n]->d_name);
        msg_debug("querying %s.\n", node);
        joydev       = get_device_data(node);
        lib_free(node);
        if (joydev != NULL) {
            joylist[joylist_index++] = joydev;
            joylist[joylist_index]   = NULL;
        }
        free(namelist[n]);
    }
    free(namelist);

    *devices = joylist;
    return (int)joylist_index;
}


static bool joydev_open(joy_device_t *joydev)
{
    joy_hwdata_t *hwdata = joydev->hwdata;
    int           fd;

    fd = open(joydev->node, O_RDONLY|O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "failed to open %s: %s.\n", joydev->node, strerror(errno));
        return false;
    }

    hwdata->fd = fd;
    return true;
}

static void joydev_close(joy_device_t *joydev)
{
    joy_hwdata_t *hwdata = joydev->hwdata;

    if (hwdata != NULL && hwdata->fd >= 0) {
        close(hwdata->fd);
    }
}

static bool joydev_poll(joy_device_t *joydev)
{
    joy_hwdata_t *hwdata = joydev->hwdata;
    ssize_t       rsize;

    if (hwdata == NULL || hwdata->fd < 0) {
        return false;
    }

    while ((rsize = read(hwdata->fd, hwdata->buffer, (size_t)(hwdata->rep_size))) == hwdata->rep_size) {
        struct hid_item  item;
        struct hid_data *data;

        data = hid_start_parse(hwdata->rep_desc, 1 << hid_input, hwdata->rep_id);
        if (data == NULL) {
            fprintf(stderr, "%s(): hid_start_parse() failed: %d (%s).\n",
                    __func__, errno, strerror(errno));
            return false;
        }

        while (hid_get_item(data, &item) > 0) {
            int32_t      value = hid_get_data(hwdata->buffer, &item);
            int          usage = HID_USAGE(item.usage);
            unsigned int page  = HID_PAGE(item.usage);

            /* do stuff */
            switch (page) {
                case HUP_GENERIC_DESKTOP:
                    switch (usage) {
                        case HUG_X:     /* fall through */
                        case HUG_Y:     /* fall through */
                        case HUG_Z:     /* fall through */
                        case HUG_RX:    /* fall through */
                        case HUG_RY:    /* fall through */
                        case HUG_RZ:    /* fall through */
                        case HUG_SLIDER:
                            /* axis */
                            joy_axis_event(joydev,
                                           joy_axis_from_code(joydev, (uint16_t)usage),
                                           value);
                            break;
                        case HUG_HAT_SWITCH:
                            joy_hat_event(joydev,
                                          joy_hat_from_code(joydev, (uint16_t)usage),
                                          value);
                            break;
                        case HUG_D_PAD_UP:      /* fall through */
                        case HUG_D_PAD_DOWN:    /* fall through */
                        case HUG_D_PAD_LEFT:    /* fall through */
                        case HUG_D_PAD_RIGHT:
                            /* D-Pad is mapped as buttons */
                            joy_button_event(joydev,
                                             joy_button_from_code(joydev, (uint16_t)usage),
                                             value);
                            break;
                        default:
                            break;
                    }
                    break;
                case HUP_BUTTON:
                    joy_button_event(joydev,
                                     joy_button_from_code(joydev, (uint16_t)usage),
                                     value);
                    break;
                default:
                    break;
            }
        }
        hid_end_parse(data);
    }

    if (rsize != -1 && errno != EAGAIN) {
        fprintf(stderr, "%s(): warning: weird report size: %zd: %s\n",
                __func__, rsize, strerror(errno));
    }
    return true;
}


/** \brief  Register BSD joystick driver
 *
 * \return  \c true
 */
bool joy_arch_init(void)
{
    joy_driver_t driver = {
        .open        = joydev_open,
        .close       = joydev_close,
        .poll        = joydev_poll,
        .hwdata_free = joy_hwdata_free
    };

    joy_driver_register(&driver);
    return true;
}


bool joy_arch_device_create_default_mapping(joy_device_t *joydev)
{
    joy_axis_t    *axis;
    joy_button_t  *button;

    if (joydev->capabilities == JOY_CAPS_NONE) {
        return false;
    }
    if (joydev->num_buttons < 1u) {
        return false;
    }

    /* prefer D-Pad for direction pins */
    if (joydev->num_buttons >= 5u && has_dpad(joydev)) {
        msg_debug("using D-Pad for joystick direction pins.\n");

        for (size_t d = 0; d < ARRAY_LEN(dpad_pins); d++) {
            button  = joy_button_from_code(joydev, dpad_pins[d].code);
            if (button == NULL) {
                /* shouldn't happen */
                msg_error("error: expected to find button for D-Pad 0x%02x\n",
                          (unsigned int)dpad_pins[d].code);
                return false;
            }
            button->mapping.action     = JOY_ACTION_JOYSTICK;
            button->mapping.target.pin = dpad_pins[d].pin;
        }
    } else if (joydev->num_axes >= 2) {
        msg_debug("using axes X & Y for joystick direction pins.\n");

        /* Y: up/down */
        axis = joy_axis_from_code(joydev, HUG_Y);
        if (axis == NULL) {
            msg_error("expected to find Y axis (0x%02x)\n", HUG_Y);
            return false;
        }
        axis->mapping.negative.action     = JOY_ACTION_JOYSTICK;
        axis->mapping.negative.target.pin = JOYSTICK_DIRECTION_UP;
        axis->mapping.positive.action     = JOY_ACTION_JOYSTICK;
        axis->mapping.positive.target.pin = JOYSTICK_DIRECTION_DOWN;

        /* X: left/right */
        axis = joy_axis_from_code(joydev, HUG_X);
        if (axis == NULL) {
            msg_error("expected to find X axis (0x%02x)\n", HUG_X);
            return false;
        }
        axis->mapping.negative.action     = JOY_ACTION_JOYSTICK;
        axis->mapping.negative.target.pin = JOYSTICK_DIRECTION_LEFT;
        axis->mapping.positive.action     = JOY_ACTION_JOYSTICK;
        axis->mapping.positive.target.pin = JOYSTICK_DIRECTION_RIGHT;
    }

    /* map Button_1 to fire */
    button = joy_button_from_code(joydev, 1u);
    if (button == NULL) {
        msg_error("expected to find Button_1 (0x01)\n");
        return false;
    }
    button->mapping.action     = JOY_ACTION_JOYSTICK;
    button->mapping.target.pin = 16;

    return true;
}


void joy_arch_shutdown(void)
{
}
