
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "lib.h"

#include "joyapi.h"


joy_device_t *joy_device_new(void)
{
    joy_device_t *dev = lib_malloc(sizeof *dev);

    dev->name        = NULL;
    dev->node        = NULL;
    dev->vendor      = 0;
    dev->product     = 0;

    dev->num_buttons = 0;
    dev->num_axes    = 0;
    dev->num_hats    = 0;
    dev->num_balls   = 0;

    dev->buttons     = NULL;
    dev->axes        = NULL;

    dev->priv        = NULL;

    return dev;
}


void joy_device_free(joy_device_t *dev)
{
    lib_free(dev->name);
    lib_free(dev->node);
    lib_free(dev->axes);
    lib_free(dev->buttons);
    lib_free(dev);
}
