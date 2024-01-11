
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

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


#define null_str(s) ((s) != NULL ? (s) : "(null)")

/** \brief  Print information on joystick device on stdout
 *
 * \param[in]   dev     joystick device
 * \param[in]   verbose be verbose
 */
void joy_device_dump(const joy_device_t *dev, bool verbose)
{
    if (verbose) {
        printf("name   : %s\n", null_str(dev->name));
        printf("node   : %s\n", null_str(dev->node));
        printf("vendor : %04"PRIx16"\n", dev->vendor);
        printf("product: %04"PRIx16"\n", dev->product);
        printf("buttons: %"PRIu32"\n", dev->num_buttons);
        printf("axes   : %"PRIu32"\n", dev->num_axes);
        printf("hats   : %"PRIu32"\n", dev->num_hats);
    } else {
        printf("%s (%"PRIu32" %s, %"PRIu32" %s, %"PRIu32" %s)\n",
               null_str(dev->name),
               dev->num_buttons, dev->num_buttons == 1u ? "button" : "buttons",
               dev->num_axes, dev->num_axes == 1u ? "axis" : "axes",
               dev->num_hats, dev->num_hats == 1u ? "hat" : "hats");
    }
}
