/** \file   joyapi.h
 * \brief   Shared joystick code, arch-agnostic
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 *
 * This file contains code that will go into (or replace) VICE's generic
 * joystick code in \c src/joyport/joystick.c.
 *
 * Still a work in progress.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>

#include "lib.h"

#include "joyapi.h"


#define null_str(s) ((s) != NULL ? (s) : "(null)")

extern bool debug;
extern bool verbose;

static const char *joy_direction_names[16] = {
    "None",         /* 0x00: - */
    "North",        /* 0x01: Up */
    "South",        /* 0x02: Down */
    "(invalid)",    /* 0x03: Up + Down */
    "West",         /* 0x04: Left */
    "Northwest",    /* 0x05: Up + Left */
    "Southwest",    /* 0x06: Down + Left*/
    "(invalid)",    /* 0x07: Up + Down + Left */
    "East",         /* 0x08: Right */
    "Northeast",    /* 0x09: Up + Right */
    "Southeast",    /* 0x0a: Down + Right */
    "(invalid)",    /* 0x0b: Up + Down + Right */
    "(invalid)",    /* 0x0c: Left + Right */
    "(invalid)",    /* 0x0d: Up + Left + Right */
    "(invalid)",    /* 0x0e: Down + Left + Right */
    "(invalid)",    /* 0x0f: Up + Down + Left + Right */
};


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
    driver.open      = drv->open;
    driver.close     = drv->close;
    driver.poll      = drv->poll;
    driver.priv_free = drv->priv_free;
}


/** \brief  Free device list and all its associated resources
 *
 * \param[in]   devices joystick device list
 */
void joy_device_list_free(joy_device_t **devices)
{
    msg_debug("Called\n");
    if (devices != NULL) {
        for (size_t i = 0; devices[i] != NULL; i++) {
            msg_debug("freeing device %zu: %s\n", i, devices[i]->name);
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

    dev->name         = NULL;
    dev->node         = NULL;
    dev->vendor       = 0;
    dev->product      = 0;
    dev->version      = 0;

    dev->num_buttons  = 0;
    dev->num_axes     = 0;
    dev->num_hats     = 0;

    dev->buttons      = NULL;
    dev->axes         = NULL;
    dev->hats         = NULL;

    dev->port         = -1;  /* unassigned */
    dev->capabilities = JOY_CAPS_NONE;  /* cannot be mapped to any emulated input */

    dev->priv         = NULL;

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
    if (driver.priv_free != NULL && joydev->priv != NULL) {
        driver.priv_free(joydev->priv);
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



/** \brief  Print information on joystick device on stdout
 *
 * \param[in]   joydev  joystick device
 */
void joy_device_dump(const joy_device_t *joydev)
{
    if (verbose) {
        printf("name       : %s\n",          null_str(joydev->name));
        printf("node       : %s\n",          null_str(joydev->node));
        printf("vendor     : %04"PRIx16"\n", joydev->vendor);
        printf("product    : %04"PRIx16"\n", joydev->product);
        printf("version    : %04"PRIx16"\n", joydev->version);
        printf("buttons    : %"PRIu32"\n",   joydev->num_buttons);
        printf("axes       : %"PRIu32"\n",   joydev->num_axes);
        printf("hats       : %"PRIu32"\n",   joydev->num_hats);
        printf("capabilites:");
        if (joydev->capabilities & JOY_CAPS_PADDLE) {
            printf(" paddle");
        }
        if (joydev->capabilities & JOY_CAPS_JOYSTICK) {
            printf(" joystick");
        }
        if (joydev->capabilities & JOY_CAPS_MOUSE) {
            printf(" mouse");
        }
        if (joydev->capabilities & JOY_CAPS_KOALA) {
            printf(" koala");
        }
        putchar('\n');
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


/** \brief  Get axis name for joystick device
 *
 * \param[in]   joydev  joystick device
 * \param[in]   axis    axis code
 *
 * \return  axis name or \a NULL when \a axis is invalid
 */
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


/** \brief  Get button name for joystick device
 *
 * \param[in]   joydev  joystick device
 * \param[in]   button  button code
 *
 * \return  button name or \a NULL when \a button is invalid
 */
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


/** \brief  Get hat name for joystick device
 *
 * \param[in]   joydev  joystick device
 * \param[in]   hat     hat code
 *
 * \return  hat name or \a NULL when \a hat is invalid
 */
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


#define joy_direction_name(mask) (joy_direction_names[mask & 0x0f])


void joy_calibration_init(joy_calibration_t *calibration)
{
    calibration->threshold_neg = 0;
    calibration->threshold_pos = 0;
    calibration->deadzone_neg  = 0;
    calibration->deadzone_pos  = 0;
    calibration->invert        = false;
}

/** \brief  Initialize joystick mapping object to default values
 *
 * \param[in]   mapping joystick mapping
 */
void joy_mapping_init(joy_mapping_t *mapping)
{
    mapping->action = JOY_ACTION_NONE;
    memset(&(mapping->target), 0, sizeof mapping->target);
    joy_calibration_init(&(mapping->calibration));
}


/** \brief  Initialize joystick axis object to default values
 *
 * \param[in]   axis    joystick axis object
 */
void joy_axis_init(joy_axis_t *axis)
{
    axis->code        = 0;
    axis->name        = NULL;
    axis->prev        = 0;
    axis->minimum     = INT16_MIN;
    axis->maximum     = INT16_MAX;
    axis->fuzz        = 0;
    axis->flat        = 0;
    axis->resolution  = 1;
    axis->granularity = 1;
    joy_mapping_init(&(axis->mapping.pin[JOY_AXIS_IDX_NEG]));
    joy_mapping_init(&(axis->mapping.pin[JOY_AXIS_IDX_POS]));
}


/** \brief  Initialize joystick button object to default values
 *
 * \param[in]   button  joystick button object
 */
void joy_button_init(joy_button_t *button)
{
    button->code = 0;
    button->name = NULL;
    button->prev = 0;
    joy_mapping_init(&(button->mapping));
}


/** \brief  Initialize joystick hat object to default values
 *
 * \param[in]   hat     joystick hat object
 */
void joy_hat_init(joy_hat_t *hat)
{
    hat->name = NULL;
    hat->code = 0;
    hat->prev = 0;
    joy_axis_init(&(hat->x));
    joy_axis_init(&(hat->y));
    for (size_t i = 0; i < ARRAY_LEN(hat->hat_map); i++) {
        hat->hat_map[i] = JOY_HAT_NEUTRAL;
    }
    joy_mapping_init(&(hat->mapping));
}


joy_axis_t *joy_axis_from_code(joy_device_t *joydev, uint16_t code)
{
    for (uint32_t a = 0; a < joydev->num_axes; a++) {
        if (joydev->axes[a].code == code) {
            return &(joydev->axes[a]);
        }
    }
    return NULL;
}


joy_button_t *joy_button_from_code(joy_device_t *joydev, uint16_t code)
{
    for (uint32_t b = 0; b < joydev->num_buttons; b++) {
        if (joydev->buttons[b].code == code) {
            return &(joydev->buttons[b]);
        }
    }
    return NULL;
}


joy_hat_t *joy_hat_from_code(joy_device_t *joydev, uint16_t code)
{
    for (uint32_t h = 0; h < joydev->num_hats; h++) {
        if (joydev->hats[h].code == code) {
            return &(joydev->hats[h]);
        }
    }
    return NULL;
}




/** \brief  Perform joystick event
 *
 * \param[in]   joydev  joystick device
 * \param[in]   event   event data
 * \param[in]   value   event value
 */
static void joy_perform_event(joy_device_t  *joydev,
                              joy_mapping_t *event,
                              int32_t        value)
{
    joy_key_map_t *key;

    switch (event->action) {
        case JOY_ACTION_NONE:
            printf("event: port %d - NONE - value: %"PRId32"\n",
                   joydev->port, value);
            break;
        case JOY_ACTION_JOYSTICK:
            printf("event: port %d - JOYSTICK - pin: %d, value: %"PRId32"\n",
                   joydev->port, event->target.pin, value);
            break;
        case JOY_ACTION_KEYBOARD:
            key = &(event->target.key);
            printf("event: port %d - KEYBOARD - row: %d, column: %d, flags: %02"PRIx32", value: %"PRId32"\n",
                   joydev->port, key->row, key->column, key->flags, value);
            break;
        case JOY_ACTION_POT_AXIS:
            printf("event: port %d: - POT %c - value: %02"PRIx32"\n",
                   joydev->port, event->target.pot == JOY_POTX ? 'X' : 'Y', value);
            break;
        case JOY_ACTION_UI_ACTION:
            printf("event: UI ACTION: id: %"PRId32"\n", value);
            break;
        case JOY_ACTION_UI_ACTIVATE:
            printf("event: UI ACTIVATE\n");
            break;
        default:
            break;
    }
}


/** \brief  Joystick axis event
 *
 * \param[in]   joydev  joystick device triggering the event
 * \param[in]   axis    axis object
 * \param[in]   value   axis value
 */
void joy_axis_event(joy_device_t *joydev, joy_axis_t *axis, joystick_axis_value_t value)
{
    if (axis == NULL) {
        msg_error("`axis` is NULL\n");
        return;
    }

    printf("axis event: %s: %s (%"PRIx16"), value: %"PRId32"\n",
           joydev->name, axis->name, axis->code, value);
}


/** \brief  Joystick button event
 *
 * \param[in]   joydev  joystick device triggering the event
 * \param[in]   button  button object
 * \param[in]   value   button value
 */
void joy_button_event(joy_device_t *joydev, joy_button_t *button, int32_t value)
{
    if (button == NULL) {
        msg_error("error: `button` is NULL\n");
        return;
    }

    printf("button event: %s: %s (%"PRIx16"), value: %"PRId32"\n",
           joydev->name, button->name, button->code, value);

    joy_perform_event(joydev, &(button->mapping), value);

}


/** \brief  Joystick hat event
 *
 * \param[in]   joydev  joystick device triggering the event
 * \param[in]   hat     hat object
 * \param[in]   value   hat value
 */
void joy_hat_event(joy_device_t *joydev, joy_hat_t *hat, int32_t value)
{
    if (hat == NULL) {
        msg_error("`hat` is NULL\n");
        return;
    }

    printf("hat event: %s: %s (%"PRIx16"), value: %"PRId32": %s\n",
           joydev->name, hat->name, hat->code, value,
           joy_direction_name((uint32_t)value));

    /* TODO: latch/unlatch pins */

    joy_perform_event(joydev, &(hat->mapping), value);
}


/** \brief  Open joystick device for polling
 *
 * \param[in]   joydev  joystick device
 *
 * \return  \c true on success
 */
bool joy_open(joy_device_t *joydev)
{
    bool result;

    msg_debug("called\n");

    if (joydev == NULL) {
        msg_error("`joydev` is NULL\n");
        return false;
    }
    if (driver.open == NULL) {
        msg_error("no open() callback registered\n");
        return false;
    }

    msg_debug("calling driver.open()\n");
    result = driver.open(joydev);
    msg_debug("%s\n", result ? "OK" : "failed");

    return result;
}


/** \brief  Close joystick device
 *
 * \param[in]   joydev  joystick device
 */
void joy_close(joy_device_t *joydev)
{
    msg_debug("called\n");
    if (joydev == NULL) {
        msg_error("`joydev` is NULL\n");
    } else if (driver.close == NULL) {
        msg_error("no close() callback registered\n");
    } else {
        msg_debug("calling driver.close()\n");
        driver.close(joydev);
    }
}


/** \brief  Poll joystick device for input
 *
 * \param[in]   joydev  joystick device
 *
 * \return  \c false on fatal error
 */
bool joy_poll(joy_device_t *joydev)
{
    msg_debug("called\n");
    if (joydev == NULL) {
        msg_error("`joydev` is NULL\n");
        return false;
    }
    if (driver.poll == NULL) {
        msg_error("no poll() callback registered\n");
        return false;
    }
    return driver.poll(joydev);
}


/** \brief  Determine required inputs for emulated device classes
 *
 * Check inputs in \a joydev to determine which class of emulated device can
 * be mapped to the device.
 *
 * \param[in]   joydev  joystick device
 */
uint32_t joy_device_set_capabilities(joy_device_t *joydev)
{
    uint32_t caps = 0;

    if (joydev->num_axes >= 1u && joydev->num_buttons >= 1u) {
        caps |= JOY_CAPS_PADDLE;
    }
    if (joydev->num_axes >= 2u && joydev->num_buttons >= 2u) {
        caps |= JOY_CAPS_MOUSE|JOY_CAPS_KOALA;
    }
    if ((joydev->num_axes >= 2u && joydev->num_buttons >= 1u) ||
        (joydev->num_hats >= 1u && joydev->num_buttons >= 1u) ||
        (joydev->num_buttons >= 5u)) {
        caps |= JOY_CAPS_JOYSTICK;
    }
    joydev->capabilities = caps;
    return caps;
}


int joy_device_list_init(joy_device_t ***devices)
{
    int count;

    *devices = NULL;
    count = joy_arch_device_list_init(devices);
    if (count <= 0) {
        return count;
    }
    for (int i = 0; i < count; i++) {
        joy_device_t *joydev = (*devices)[i];

        if (joy_device_set_capabilities(joydev) == JOY_CAPS_NONE) {
            msg_debug("TODO: insufficient capabilities: reject device\n");
        }

        /* create default mapping */
        joy_arch_device_create_default_mapping(joydev);
    }
    return count;
}


bool joy_init(void)
{
    /* just this for now */
    return joy_arch_init();
}
