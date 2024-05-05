/** \brief  joy.c
 * \brief   SDL joystick interface
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#include "joyapi.h"
#include "lib.h"

/* external symbols */
extern bool debug;
extern bool verbose;

/** \brief  SDL-specific device data */
typedef struct hwdata_s {
    SDL_Joystick   *dev;    /**< SDL joystick handle */
    SDL_JoystickID  id;     /**< SDL joystick instance ID */
    int             index;  /**< index used for SDL_JoystickOpen() */
} hwdata_t;


/** \brief  We've properly initialized SDL's joystick subsystem
 *
 * Used by \c joy_arch_shutdown() to call \c SQL_Quit() if initialization was
 * successful.
 */
static bool sdl_initialized = false;


/** \brief  Allocate SDL-specific data for joystick devices
 *
 * \return   new hwdata object, initialized
 */
static hwdata_t *hwdata_new(void)
{
    hwdata_t *hwdata = lib_malloc(sizeof *hwdata);

    hwdata->dev    = NULL;
    hwdata->id     = 0;
    hwdata->index  = -1;
    return hwdata;
}

/** \brief  Free SDL-specific data for joystick device
 *
 * Should not be called directly, will be called by \c joy_device_free().
 *
 * \param[in]   hwdata  SDL-specific data
 */
static void hwdata_free(void *hwdata)
{
    hwdata_t *hw = hwdata;

    if (hw != NULL) {
        if (hw->dev != NULL) {
            SDL_JoystickClose(hw->dev);
        }
        lib_free(hw);
    }
}

/** \brief  Translate SDL hat direction to VICE hat direction
 *
 * \param[in]   value   SDL hat direction
 *
 * \return  VICE hat direction
 */
static int32_t sdl_hat_direction_to_vice(uint8_t value)
{
    int32_t direction = 0;

    if (value & SDL_HAT_UP) {
        direction |= JOYSTICK_DIRECTION_UP;
    }
    if (value & SDL_HAT_DOWN) {
        direction |= JOYSTICK_DIRECTION_DOWN;
    }
    if (value & SDL_HAT_LEFT) {
        direction |= JOYSTICK_DIRECTION_LEFT;
    }
    if (value & SDL_HAT_RIGHT) {
        direction |= JOYSTICK_DIRECTION_RIGHT;
    }
    return direction;
}

/** \brief  Scan device for axes and their properties
 *
 * \param[in]   joydev  VICE joystick device
 * \param[in]   sdldev  SDL joystick device
 *
 * \return  \c true on success
 */
static bool scan_axes(joy_device_t *joydev, SDL_Joystick *sdldev)
{
    int naxes = SDL_JoystickNumAxes(sdldev);

    if (naxes < 0) {
        msg_error("failed to get number of axes: %s\n", SDL_GetError());
        false;
    }
    joydev->num_axes = (uint32_t)naxes;
    if (naxes == 0) {
        return true;
    }

    joydev->axes = lib_malloc((size_t)naxes * sizeof *(joydev->axes));
    for (int a = 0; a < naxes; a++) {
        joy_axis_t *axis = &joydev->axes[a];

        joy_axis_init(axis);
        axis->code = (uint16_t)a;
        axis->name = lib_msprintf("Axis_%d", a);
        axis->minimum = SDL_JOYSTICK_AXIS_MIN;
        axis->maximum = SDL_JOYSTICK_AXIS_MAX;
    }
    return true;
}

/** \brief  Scan device for buttons and their properties
 *
 * \param[in]   joydev  VICE joystick device
 * \param[in]   sdldev  SDL joystick device
 *
 * \return  \c true on success
 */
static bool scan_buttons(joy_device_t *joydev, SDL_Joystick *sdldev)
{
    int nbuttons = SDL_JoystickNumButtons(sdldev);

    if (nbuttons < 0) {
        msg_error("failed to get number of buttons: %s\n", SDL_GetError());
        false;
    }
    joydev->num_buttons = (uint32_t)nbuttons;
    if (nbuttons == 0) {
        return true;
    }

    joydev->buttons = lib_malloc((size_t)nbuttons * sizeof *joydev->buttons);
    for (int b = 0; b < nbuttons; b++) {
        joy_button_t *button = &joydev->buttons[b];

        joy_button_init(button);
        button->code = (uint16_t)b;
        button->name = lib_msprintf("Button_%d", b);
    }
    return true;
}

/** \brief  Scan device for hats and their properties
 *
 * \param[in]   joydev  VICE joystick device
 * \param[in]   sdldev  SDL joystick device
 *
 * \return  \c true on success
 */
static bool scan_hats(joy_device_t *joydev, SDL_Joystick *sdldev)
{
    int nhats = SDL_JoystickNumHats(sdldev);

    if (nhats < 0) {
        msg_error("failed to get number of hats: %s\n", SDL_GetError());
        return false;
    }
    joydev->num_hats = (uint32_t)nhats;
    if (nhats == 0) {
        return true;
    }

    joydev->hats = lib_malloc((size_t)nhats * sizeof *joydev->hats);
    for (int h = 0; h < nhats; h++) {
        joy_hat_t *hat = &joydev->hats[h];

        joy_hat_init(hat);
        hat->code = (uint16_t)h;
        hat->name = lib_msprintf("Hat_%d", h);
    }
    return true;
}

/** \brief  Get device information and create joystick object
 *
 * Obtain device name, node, vendor, product, product version, axes, buttons
 * and hats.
 *
 * \param[in]   sdldev  SDL joystick device
 * \param[in]   index   index of device according to SDL
 *
 * \return  new joystick object, or \c NULL on failure
 */
static joy_device_t *get_device_data(SDL_Joystick *sdldev, int index)
{
    joy_device_t *joydev;
    hwdata_t     *hwdata;

    hwdata          = hwdata_new();
    hwdata->index   = index;
    hwdata->id      = SDL_JoystickInstanceID(sdldev);

    joydev          = joy_device_new();
    joydev->hwdata  = hwdata;
    joydev->name    = lib_strdup(SDL_JoystickName(sdldev));
    joydev->node    = lib_strdup(SDL_JoystickPath(sdldev));
    joydev->vendor  = SDL_JoystickGetVendor(sdldev);
    joydev->product = SDL_JoystickGetProduct(sdldev);
    joydev->version = SDL_JoystickGetProductVersion(sdldev);

    if (!scan_axes(joydev, sdldev)) {
        goto exit_err;
    }
    if (!scan_buttons(joydev, sdldev)) {
        goto exit_err;
    }
    if (!scan_hats(joydev, sdldev)) {
        goto exit_err;
    }

    return joydev;

exit_err:
    /* clean up */
    hwdata_free(hwdata);
    joy_device_free(joydev);
    return NULL;
}


/** \brief  Generate list of connected joystick devices
 *
 * \param[out]  devices list of joystick devices
 *
 * \return  number of devices in \a devices, or -1 on error
 */
int joy_arch_device_list_init(joy_device_t ***devices)
{
    joy_device_t **joylist;
    int            joy_idx;
    int            num_sdl;

    num_sdl = SDL_NumJoysticks();
#if 0
    printf("%s(): %d %s connected\n",
           __func__, num_sdl, num_sdl == 1 ? "joystick" : "joysticks");
#endif
    if (num_sdl == 0) {
        *devices = NULL;
        return 0;
    }

    joylist = lib_malloc((size_t)(num_sdl + 1) * sizeof *joylist);
    joy_idx = 0;

    for (int n = 0; n < num_sdl; n++) {
        SDL_Joystick *sdldev = SDL_JoystickOpen(n);

        if (sdldev == NULL) {
            msg_error("failed to open joystick %d: %s\n", n, SDL_GetError());
        } else {
            joy_device_t *joydev = get_device_data(sdldev, n);

            if (joydev != NULL) {
                joylist[joy_idx++] = joydev;
            }
            SDL_JoystickClose(sdldev);
        }
    }

    joylist[joy_idx] = NULL;
    *devices = joylist;

    return joy_idx;
}


/** \brief  Create default mapping for a device
 *
 * \param[in]   joydev  joystick device
 *
 * \return  \c true on success
 */
bool joy_arch_device_create_default_mapping(joy_device_t *joydev)
{
    joy_mapping_t *mapping;
    joy_axis_t    *axis;
    joy_button_t  *button;
    joy_hat_t     *hat;

    if (joydev->capabilities == JOY_CAPS_NONE) {
        msg_error("no capabilities for device %s\n", joydev->name);
        return false;
    }

    if (joydev->capabilities & JOY_CAPS_JOYSTICK) {
        if (joydev->num_hats >= 1u) {
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


/** \brief  Open joystick device for polling
 *
 * Implements the driver's \c joy_open() callback.
 *
 * \param[in]   joydev  joystick device
 *
 * \return  \c true on success
 */
static bool joydev_open(joy_device_t *joydev)
{
    hwdata_t *hwdata = joydev->hwdata;

    hwdata->dev = SDL_JoystickOpen(hwdata->index);
    if (hwdata->dev == NULL) {
        msg_error("failed to open joystick device %d (\"%s\"): %s\n",
                  hwdata->index, joydev->name, SDL_GetError());
        return false;
    }
    return true;
}

/** \brief  Close joystick device
 *
 * Implements the driver's \c joy_close() method.
 *
 * \param[in]   joydev  joystick device
 */
static void joydev_close(joy_device_t *joydev)
{
    hwdata_t *hwdata = joydev->hwdata;

    if (hwdata != NULL) {
        if (hwdata->dev != NULL) {
            SDL_JoystickClose(hwdata->dev);
            hwdata->dev = NULL;
        }
    }
}

/** \brief  Poll joystick device and trigger joystick events
 *
 * \param[in]   joydev  joystick device
 *
 * \return  \c true on success, \c false indicates an error and signals the
 *          joystick core code to stop polling
 */
static bool joydev_poll(joy_device_t *joydev)
{
    joy_axis_t   *axis;
    joy_button_t *button;
    joy_hat_t    *hat;
    hwdata_t     *hwdata;
    SDL_Event     event;
    uint16_t      code;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_JOYAXISMOTION:
                code = event.jaxis.axis;
                axis = joy_axis_from_code(joydev, code);
                if (axis == NULL) {
                    msg_error("invalid axis code %04x\n", (unsigned int)code);
                    return false;
                }
                msg_debug("EVENT: joy axis %d (%s) motion: %d\n",
                          (int)code, axis->name, (int)event.jaxis.value);
                joy_axis_event(joydev, axis, event.jaxis.value);
                break;

            case SDL_JOYBUTTONDOWN: /* fall through */
            case SDL_JOYBUTTONUP:
                code   = event.jbutton.button;
                button = joy_button_from_code(joydev, code);
                if (button == NULL) {
                    msg_error("invalid button code %04x\n", (unsigned int)code);
                    return false;
                }
                msg_debug("EVENT: joy button %d (%s) %s\n",
                          (int)code, button->name,
                          event.jbutton.state == SDL_PRESSED ? "pressed" : "released");
                joy_button_event(joydev, button, event.jbutton.state);
                break;

            case SDL_JOYHATMOTION:
                code = event.jhat.hat;
                hat  = joy_hat_from_code(joydev, code);
                if (hat == NULL) {
                    msg_error("invalid hat code %04x\n", (unsigned int)code);
                    return false;
                }
                msg_debug("EVENT: hat %d (%s) motion: %d\n",
                          (int)code, hat->name, event.jhat.value);
                joy_hat_event(joydev, hat, sdl_hat_direction_to_vice(event.jhat.value));
                break;

            case SDL_JOYDEVICEADDED:
                msg_debug("EVENT: joy device ADDED: index = %d\n", event.jdevice.which);
                break;

            case SDL_JOYDEVICEREMOVED:
                hwdata = joydev->hwdata;
                if (hwdata->id == event.jdevice.which) {
                    msg_debug("EVENT: joy device %s REMOVED\n", joydev->name);
                    /* the current device was removed */
                    return false;
                }
                break;

            default:
                /* ignore event */
                break;
        }
    }
    return true;
}


/** \brief  Initialize SDL2 joystick subsystem and register driver
 *
 * \return  \c true on success
 */
bool joy_arch_init(void)
{
    joy_driver_t driver = {
        .open        = joydev_open,
        .close       = joydev_close,
        .poll        = joydev_poll,
        .hwdata_free = hwdata_free
    };

    msg_debug("Initializing SDL2 ...\n");
    if (SDL_Init(SDL_INIT_JOYSTICK) != 0) {
        msg_error("Failed to initialize SDL2: %s\n", SDL_GetError());
        return false;
    }
    sdl_initialized = true;
    msg_debug("OK\n");

    joy_driver_register(&driver);
    return true;
}


/** \brief  Shut down driver and its associated resources
 *
 * Currently only calls \c SDL_Quit() if SDL was previously successfully
 * initialized.
 */
void joy_arch_shutdown(void)
{
    if (sdl_initialized) {
        SDL_Quit();
    }
}
