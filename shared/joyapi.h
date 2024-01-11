/** \file   joyapi.h
 * \brief   Joystick interface API
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef JOYAPI_H
#define JOYAPI_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct joy_axis_s {
    uint16_t code;
    int32_t  minimum;
    int32_t  maximum;
    int32_t  fuzz;
    int32_t  flat;
    int32_t  resolution;
} joy_axis_t;

typedef struct joy_hat_s {
    joy_axis_t x;
    joy_axis_t y;
} joy_hat_t;

typedef struct joy_device_s {
    char       *name;
    char       *node;
    uint16_t    vendor;
    uint16_t    product;

    uint32_t    num_buttons;
    uint32_t    num_axes;
    uint32_t    num_hats;
    uint32_t    num_balls;

    uint16_t   *buttons;
    joy_axis_t *axes;
    joy_hat_t  *hats;

    void       *priv;
} joy_device_t;


int  joy_get_devices(joy_device_t ***devices);
void joy_free_devices(joy_device_t **devices);

joy_device_t *joy_device_new(void);
void          joy_device_free(joy_device_t *dev);
void          joy_device_dump(const joy_device_t *dev, bool verbose);

const char   *joy_device_get_button_name(const joy_device_t *dev, uint16_t code);
const char   *joy_device_get_axis_name  (const joy_device_t *dev, uint16_t code);
const char   *joy_device_get_hat_name   (const joy_device_t *dev, uint16_t code);

#endif
