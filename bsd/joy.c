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
#endif

#include "lib.h"
#include "joyapi.h"


#define ROOT_NODE       "/dev"
#define ROOT_NODE_LEN   4u
#define NODE_PREFIX     "uhid"
#define NODE_PREFIX_LEN 4u


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

static void add_joy_button(button_iter_t *iter, int usage)
{
    joy_button_t *button;
    char          name[64];

    LIST_ITER_RESIZE(iter);

    snprintf(name, sizeof name, "Button %d", usage);

    button = &(iter->list[iter->index++]);
    button->code = (uint16_t)usage;
    button->name = lib_strdup(name);
}


static joy_device_t *get_device_data(const char *node)
{
    joy_device_t           *joydev;
    struct usb_device_info  devinfo;
    report_desc_t           report;
    int                     rep_id;
    struct hid_data        *hdata;
    struct hid_item         hitem;
    char                   *name;
    int                     fd;
    button_iter_t           button_iter;

    fd = open(node, O_RDONLY|O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "%s(): failed to open %s.\n", __func__, node);
        return NULL;
    }

    /* get device info for vendor and product */
    if (ioctl(fd, USB_GET_DEVICEINFO, &devinfo) < 0) {
        fprintf(stderr, "%s(): ioctl failed: %s.\n",
                __func__, strerror(errno));
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

    /* get buttons for device */
    LIST_ITER_INIT(&button_iter, 16u);

    hdata = hid_start_parse(report, 1 << hid_input, rep_id);
    while (hid_get_item(hdata, &hitem) > 0) {
        unsigned int page  = HID_PAGE (hitem.usage);
        int          usage = HID_USAGE(hitem.usage);

        if (page == HUP_BUTTON) {
            /* usage appears to be the button number */
            printf("%s(): adding button: data: %04x, usage: %d\n",
                   __func__, hid_get_data(hdata, &hitem), usage);
            add_joy_button(&button_iter, usage);

        }
    }
    hid_end_parse(hdata);

    joydev->buttons     = button_iter.list;
    joydev->num_buttons = (uint32_t)button_iter.index;

    hid_dispose_report_desc(report);
    close(fd);
    return joydev;
}


int joy_device_list_init(joy_device_t ***devices)
{
    struct dirent **namelist = NULL;
    joy_device_t  **joylist;
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
