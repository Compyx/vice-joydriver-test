#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include "lib.h"
#include "joyapi.h"


static int sd_select(const struct dirent *de)
{
    const char *name = de->d_name;

    // printf("%s(): got '%s' -> ", __func__, name);
    if (strlen(name) >= 5u && strncmp("uhid", name, 4u) == 0) {
        // printf("OK\n");
        return 1;
    }
    // printf("nope\n");
    return 0;
}


static char *get_full_node_path(const char *node)
{
    size_t  nlen = strlen(node);
    size_t  plen = 5u + nlen + 1u;
    char   *path = lib_malloc(plen);

    memcpy(path, "/dev", 4u);
    path[4] = '/';
    memcpy(path + 5, node, nlen + 1u);
    return path;
}

int joy_get_devices(joy_device_t ***devices)
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

    nl_count = scandir("/dev", &namelist, sd_select, NULL);
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
        joydev       = joy_device_new();
        joydev->node = node;
//        printf("%s(): found %s\n", __func__, node);

        joylist[joylist_index++] = joydev;
        joylist[joylist_index]   = NULL;

        free(namelist[n]);
    }
    free(namelist);

    *devices = joylist;
    return (int)joylist_index;

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
