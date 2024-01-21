
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>

#include "lib.h"

#include "joyapi.h"


/** \brief  Arch-specific callbacks for the joystick system
 *
 * Must be set by the arch-specific code by calling \c joy_driver_register().
 */
static joy_driver_t driver;


/** \brief  Register arch-specific callbacks for the joystick system
 *
 * \param[in]   drv joystick driver object
 */
void joy_driver_register(const joy_driver_t *drv)
{
    driver.open  = drv->open;
    driver.close = drv->close;
    driver.poll  = drv->poll;
}


/** \brief  Free device list and all its associated resources
 *
 * \param[in]   devices joystick device list
 */
void joy_device_list_free(joy_device_t **devices)
{
    if (devices != NULL) {
        for (size_t i = 0; devices[i] != NULL; i++) {
            joy_device_free(devices[i]);
        }
        lib_free(devices);
    }
}


/** \brief  Allocate and initialize new joystick device object
 *
 * All members are initialized to \c 0 or \c NULL.
 *
 * \return  new joystick device object
 */
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

    dev->buttons     = NULL;
    dev->axes        = NULL;
    dev->hats        = NULL;

    dev->priv        = NULL;

    return dev;
}


/** \brief  Free all resources associated with joystick device
 *
 * Also calls the joystick driver's \c close() function to close and cleanup
 * any arch-specific resources. The call to \c close() happens before freeing
 * any other data so that callback can still access any data it might need.
 *
 * \param[in]   joydev  joystick device
 */
void joy_device_free(joy_device_t *joydev)
{
    uint32_t i;

    /* properly close device */
    if (driver.close != NULL) {
        driver.close(joydev);
    }

    lib_free(joydev->name);
    lib_free(joydev->node);

    if (joydev->axes != NULL) {
        for (i = 0; i < joydev->num_axes; i++) {
            lib_free(joydev->axes[i].name);
        }
        lib_free(joydev->axes);
    }

    if (joydev->buttons != NULL) {
        for (i = 0; i < joydev->num_buttons; i++) {
            lib_free(joydev->buttons[i].name);
        }
        lib_free(joydev->buttons);
    }

    if (joydev->hats != NULL) {
        for (i = 0; i < joydev->num_hats; i++) {
            lib_free(joydev->hats[i].x.name);
            lib_free(joydev->hats[i].y.name);
            lib_free(joydev->hats[i].name);
        }
        lib_free(joydev->hats);
    }

    lib_free(joydev);
}


#define null_str(s) ((s) != NULL ? (s) : "(null)")

/** \brief  Print information on joystick device on stdout
 *
 * \param[in]   joydev  joystick device
 * \param[in]   verbose be verbose
 */
void joy_device_dump(const joy_device_t *joydev, bool verbose)
{
    if (verbose) {
        printf("name   : %s\n",          null_str(joydev->name));
        printf("node   : %s\n",          null_str(joydev->node));
        printf("vendor : %04"PRIx16"\n", joydev->vendor);
        printf("product: %04"PRIx16"\n", joydev->product);
        printf("buttons: %"PRIu32"\n",   joydev->num_buttons);
        printf("axes   : %"PRIu32"\n",   joydev->num_axes);
        printf("hats   : %"PRIu32"\n",   joydev->num_hats);
    } else {
        printf("%s: %s (%"PRIu32" %s, %"PRIu32" %s, %"PRIu32" %s)\n",
               null_str(joydev->node), null_str(joydev->name),
               joydev->num_buttons, joydev->num_buttons == 1u ? "button" : "buttons",
               joydev->num_axes, joydev->num_axes == 1u ? "axis" : "axes",
               joydev->num_hats, joydev->num_hats == 1u ? "hat" : "hats");
    }
}


/** \brief  Get joystick device from list by its node
 *
 * \param[in]   devices joystick device list
 * \param[in]   node    device node on the OS (or GUID on Windows)
 *
 * \return  device or \c NULL when not found
 */
joy_device_t *joy_device_get(joy_device_t **devices, const char *node)
{
    if (devices != NULL && node != NULL && *node != '\0') {
        for (size_t i = 0; devices[i] != NULL; i++) {
            if (strcmp(devices[i]->node, node) == 0) {
                return devices[i];
            }
        }
    }
    return NULL;
}


const char *joy_device_get_axis_name(const joy_device_t *joydev, uint16_t axis)
{
    if (joydev != NULL) {
        for (size_t i = 0; i < joydev->num_axes; i++) {
            if (joydev->axes[i].code == axis) {
                return joydev->axes[i].name;
            }
        }
    }
    return NULL;
}


const char *joy_device_get_button_name(const joy_device_t *joydev, uint16_t button)
{
    if (joydev != NULL) {
        for (size_t i = 0; i < joydev->num_buttons; i++) {
            if (joydev->buttons[i].code == button) {
                return joydev->buttons[i].name;
            }
        }
    }
    return NULL;
}


const char *joy_device_get_hat_name(const joy_device_t *joydev, uint16_t hat)
{
    if (joydev != NULL) {
        for (size_t i = 0; i < joydev->num_hats; i++) {
            if (joydev->hats[i].code == hat) {
                return joydev->hats[i].name;
            }
        }
    }
    return NULL;
}




/** \brief  Initialize joystick axis object to default values
 *
 * \param[in]   axis    joystick axis object
 */
void joy_axis_init(joy_axis_t *axis)
{
    axis->code        = 0;
    axis->name        = NULL;
    axis->minimum     = INT16_MIN;
    axis->maximum     = INT16_MAX;
    axis->fuzz        = 0;
    axis->flat        = 0;
    axis->resolution  = 1;
    axis->granularity = 1;
}


/** \brief  Initialize joystick button object to default values
 *
 * \param[in]   button  joystick button object
 */
void joy_button_init(joy_button_t *button)
{
    button->code = 0;
    button->name = NULL;
}


/** \brief  Initialize joystick hat object to default values
 *
 * \param[in]   hat     joystick hat object
 */
void joy_hat_init(joy_hat_t *hat)
{
    hat->name = NULL;
    hat->code = 0;
    joy_axis_init(&(hat->x));
    joy_axis_init(&(hat->y));
    for (size_t i = 0; i < sizeof hat->hat_map; i++) {
        hat->hat_map[i] = JOY_HAT_NEUTRAL;
    }
}



void joy_axis_event(const joy_device_t *joydev, uint16_t axis, int32_t value)
{
    printf("axis event: %s: %s (%"PRIx16"), value: %"PRId32"\n",
           joydev->name, joy_device_get_axis_name(joydev, axis), axis, value);
}


void joy_button_event(const joy_device_t *joydev, uint16_t button, int32_t value)
{
    printf("button event: %s: %s (%"PRIx16"), value: %"PRId32"\n",
           joydev->name, joy_device_get_button_name(joydev, button), button, value);

}


void joy_hat_event(const joy_device_t *joydev, uint16_t hat, int32_t value)
{
    printf("hat event: %s: %s (%"PRIx16"), value: %"PRId32"\n",
           joydev->name, joy_device_get_hat_name(joydev, hat), hat, value);
}


bool joy_open(joy_device_t *joydev)
{
    if (joydev == NULL) {
        fprintf(stderr, "%s(): error: `joydev` is null.\n", __func__);
        return false;
    }
    if (driver.open == NULL) {
        fprintf(stderr, "%s(): error: no open() callback registered.\n", __func__);
        return false;
    }
    return driver.open(joydev);
}


void joy_close(joy_device_t *joydev)
{
    if (joydev == NULL) {
        fprintf(stderr, "%s(): error: `joydev` is null.\n", __func__);
    } else if (driver.close == NULL) {
        fprintf(stderr, "%s(): error: no close() callback registered.\n", __func__);
    } else {
        driver.open(joydev);
    }
}


bool joy_poll(joy_device_t *joydev)
{
    if (joydev == NULL) {
        fprintf(stderr, "%s(): error: `joydev` is null.\n", __func__);
        return false;
    }
    if (driver.poll == NULL) {
        fprintf(stderr, "%s(): error: no poll() callback registered.\n", __func__);
        return false;
    }
    return driver.poll(joydev);
}
