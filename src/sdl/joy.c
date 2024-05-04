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

extern bool debug;
extern bool verbose;

typedef struct hwdata_s {
    int index;
} hwdata_t;


static bool sdl_initialized = false;


static hwdata_t *hwdata_new(void)
{
    hwdata_t *hwdata = lib_malloc(sizeof *hwdata);

    hwdata->index = -1;
    return hwdata;
}

static void hwdata_free(void *hwdata)
{
    lib_free(hwdata);
}




/* minimal code to make the main program link */

bool joy_arch_init(void)
{
    printf("Initializing SDL2 ... ");
    if (SDL_Init(SDL_INIT_JOYSTICK) != 0) {
        printf("failed: %s\n", SDL_GetError());
        return false;
    }
    sdl_initialized = true;
    printf("OK\n");
    return true;
}


void joy_arch_shutdown(void)
{
    if (sdl_initialized) {
        SDL_Quit();
    }
}

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
        joy_axis_t *axis;
        char        name[64];

        axis = &joydev->axes[a];
        joy_axis_init(axis);
        snprintf(name, sizeof name - 1u, "Axis_%d", a);
        axis->code = (uint16_t)a;
        axis->name = lib_strdup(name);
        axis->minimum = INT16_MIN;
        axis->maximum = INT16_MAX;
    }

    return true;
}

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
        joy_button_t *button;
        char          name[64];

        button = &joydev->buttons[b];
        joy_button_init(button);
        snprintf(name, sizeof name - 1u, "Button_%d", b);
        button->code = (uint16_t)b;
        button->name = lib_strdup(name);
    }
    return true;
}

static bool scan_hats(joy_device_t *joydev, SDL_Joystick *sdldev)
{
    int nhats = SDL_JoystickNumHats(sdldev);

    if (nhats < 0) {
        msg_error("failed to get number of hats: %s\n", SDL_GetError());
        return false;
    }
    joydev->num_hats = (uint16_t)nhats;
    if (nhats == 0) {
        return true;
    }

    joydev->hats = lib_malloc((size_t)nhats * sizeof *joydev->hats);
    for (int h = 0; h < nhats; h++) {
        joy_hat_t *hat;
        char       name[64];

        hat = &joydev->hats[h];
        joy_hat_init(hat);
        snprintf(name, sizeof name - 1u, "Hat_%d", h);
        hat->code = (uint16_t)h;
        hat->name = lib_strdup(name);
    }

    return true;
}

static joy_device_t *get_device_data(SDL_Joystick *sdldev, int index)
{
    joy_device_t *joydev;
    hwdata_t     *hwdata;

    hwdata          = hwdata_new();
    hwdata->index   = index;

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
    hwdata_free(hwdata);
    joy_device_free(joydev);
    return NULL;
}

int joy_arch_device_list_init(joy_device_t ***devices)
{
    joy_device_t **joylist;
    int            joy_idx;
    int            num_sdl;

    num_sdl = SDL_NumJoysticks();
    printf("%s(): %d %s connected\n",
           __func__, num_sdl, num_sdl == 1 ? "joystick" : "joysticks");
    if (num_sdl == 0) {
        *devices = NULL;
        return 0;
    }

    joylist = lib_malloc((size_t)(num_sdl + 1) * sizeof *joylist);
    joy_idx = 0;

    for (int n = 0; n < num_sdl; n++) {
        SDL_Joystick *sdldev = SDL_JoystickOpen(n);

        if (sdldev == NULL) {
            fprintf(stderr, "failed to open joystick %d: %s\n", n, SDL_GetError());
        } else {
            joy_device_t *joydev;

            printf("Adding joydev %d: %s\n", n, SDL_JoystickName(sdldev));
            joydev = get_device_data(sdldev, n);
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

bool joy_arch_device_create_default_mapping(joy_device_t *joydev)
{
    (void)(joydev);
    printf("%s(): stub\n", __func__);
    return true;
}
