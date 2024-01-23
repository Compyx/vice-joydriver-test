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


typedef struct joy_priv_s {
    void          *buffer;
    report_desc_t  rep_desc;
    ssize_t        rep_size;
    int            rep_id;
    int            fd;
} joy_priv_t;


static joy_priv_t *joy_priv_new(void)
{
    joy_priv_t *priv = lib_malloc(sizeof *priv);

    priv->fd       = -1;
    priv->rep_id   = 0;
    priv->rep_size = 0;
    priv->buffer   = NULL;
    priv->rep_desc = NULL;

    return priv;
}

static void joy_priv_free(void *priv)
{
    joy_priv_t *pr = priv;
    if (pr != NULL) {
        if (pr->fd >= 0) {
            close(pr->fd);
        }
        if (pr->buffer != NULL) {
            lib_free(pr->buffer);
        }
        if (pr->rep_desc != NULL) {
            hid_dispose_report_desc(pr->rep_desc);
        }
        lib_free(pr);
    }
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
    printf("%s(): adding button %u: %s\n",
           __func__, (unsigned int)button->code, button->name);
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
    printf("%s(): adding axis %u: %s (%d - %d)\n",
           __func__, (unsigned int)axis->code, axis->name,
           axis->minimum, axis->maximum);
}

static void add_joy_hat(hat_iter_t *iter, const struct hid_item *item)
{
    joy_hat_t *hat;

    LIST_ITER_RESIZE(iter);
    hat = &(iter->list[iter->index++]);
    joy_hat_init(hat);
    hat->name = lib_strdup(hid_usage_in_page(item->usage));
    printf("%s(): adding hat %u: %s\n",
           __func__, HID_USAGE(item->usage), hat->name);
}


static joy_device_t *get_device_data(const char *node)
{
    joy_device_t           *joydev;
    joy_priv_t             *priv;
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
        fprintf(stderr, "%s(): failed to get HID report for %s: %d: %s.\n",
                __func__, node, errno, strerror(errno));
        close(fd);
        return NULL;
    }

    rep_size = hid_report_size(report, hid_input, rep_id);
    printf("%s(): report size = %d\n", __func__, rep_size);

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

    LIST_ITER_INIT(&axis_iter, 8u);
    LIST_ITER_INIT(&button_iter, 16u);
    LIST_ITER_INIT(&hat_iter, 4u);

    hdata = hid_start_parse(report, 1 << hid_input, rep_id);
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
                        printf("%s(): TODO: got HUG_HAT_SWITCH\n", __func__);
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
//    hid_dispose_report_desc(report);

    joydev->axes         = axis_iter.list;
    joydev->num_axes     = (uint32_t)axis_iter.index;
    joydev->buttons      = button_iter.list;
    joydev->num_buttons  = (uint32_t)button_iter.index;
    joydev->hats         = hat_iter.list;
    joydev->num_hats     = (uint32_t)hat_iter.index;

    priv = joy_priv_new();
    priv->fd          = -1;
    priv->rep_id   = rep_id;
    priv->rep_size = rep_size;
    priv->buffer      = lib_malloc((size_t)rep_size);
    priv->rep_desc    = report;
    joydev->priv = priv;

    close(fd);
    return joydev;
}


int joy_device_list_init(joy_device_t ***devices)
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
        printf("%s(): querying %s:\n", __func__, node);
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
    joy_priv_t *priv;
    int         fd;

    fd = open(joydev->node, O_RDONLY|O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "failed to open %s: %s.\n", joydev->node, strerror(errno));
        return false;
    }

    priv = joydev->priv;
    priv->fd     = fd;
    return true;
}

static void joydev_close(joy_device_t *joydev)
{
    joy_priv_t *priv = joydev->priv;

    if (priv != NULL && priv->fd >= 0) {
        close(priv->fd);
    }
}

static bool joydev_poll(joy_device_t *joydev)
{
    joy_priv_t *priv;
    ssize_t     rsize;

    priv = joydev->priv;
    if (priv == NULL || priv->fd < 0) {
        return false;
    }

    while ((rsize = read(priv->fd, priv->buffer, (size_t)(priv->rep_size))) == priv->rep_size) {
        struct hid_item  item;
        struct hid_data *data;

        printf("%s(): parsing report\n", __func__);

        data = hid_start_parse(priv->rep_desc, 1 << hid_input, priv->rep_id);
        while (hid_get_item(data, &item) > 0) {
            int32_t      value = hid_get_data(priv->buffer, &item);
            unsigned int usage = HID_USAGE(item.usage);
            int          page  = HID_PAGE(item.usage);

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
#if 0
                            printf("%s(): axis %d: %d\n",
                                   __func__, usage, value);
#endif
                            joy_axis_event(joydev, (uint16_t)usage, value);
                            break;
                        case HUG_HAT_SWITCH:
#if 0
                            printf("%s(): hat switch: %d\n",
                                   __func__, value);
#endif
                            joy_hat_event(joydev, (uint16_t)usage, value);
                            break;
                        case HUG_D_PAD_UP:      /* fall through */
                        case HUG_D_PAD_DOWN:    /* fall through */
                        case HUG_D_PAD_LEFT:    /* fall through */
                        case HUG_D_PAD_RIGHT:
                            /* treat D-Pad as buttons */
#if 0
                            printf("%s(): D-Pad %d: %d\n",
                                   __func__, usage, value);
#endif
                            joy_button_event(joydev, (uint16_t)usage, value);
                            break;
                        default:
                            break;
                    }
                    break;
                case HUP_BUTTON:
#if 0
                    printf("%s(): button %d: %d\n",
                           __func__, usage, value);
#endif
                    joy_button_event(joydev, (uint16_t)usage, value);
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
bool joy_init(void)
{
    joy_driver_t driver = {
        .open  = joydev_open,
        .close = joydev_close,
        .poll  = joydev_poll,
        .priv_free = joy_priv_free
    };

    joy_driver_register(&driver);
    return true;
}
