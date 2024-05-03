/** \file   joyapi.h
 * \brief   Joystick interface API
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef VICE_JOYAPI_H
#define VICE_JOYAPI_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "joyapi-types.h"

/*
 * Prototypes mark 'arch' are expected to be implemented for the arch using
 * arch-specific code.
 */

bool          joy_arch_init(void);
int           joy_arch_device_list_init(joy_device_t ***devices);
bool          joy_arch_device_create_default_mapping(joy_device_t *joydev);

/* Shared code */

bool          joy_init(void);

void          joy_driver_register(const joy_driver_t *drv);
int           joy_device_list_init     (joy_device_t ***devices);

void          joy_device_list_free(joy_device_t  **devices);

joy_device_t *joy_device_new (void);
void          joy_device_free(joy_device_t *dev);
void          joy_device_dump(const joy_device_t *dev);
joy_device_t *joy_device_get(joy_device_t **devices, const char *node);
uint32_t      joy_device_set_capabilities(joy_device_t *joydev);

const char   *joy_device_get_button_name(const joy_device_t *joydev, uint16_t code);
const char   *joy_device_get_axis_name  (const joy_device_t *joydev, uint16_t code);
const char   *joy_device_get_hat_name   (const joy_device_t *joydev, uint16_t code);

void          joy_calibration_init(joy_calibration_t *calibration);
void          joy_mapping_init    (joy_mapping_t     *mapping);
void          joy_axis_init       (joy_axis_t        *axis);
void          joy_button_init     (joy_button_t      *button);
void          joy_hat_init        (joy_hat_t         *hat);

joy_axis_t   *joy_axis_from_code  (joy_device_t *joydev, uint16_t code);
joy_axis_t   *joy_axis_from_name  (joy_device_t *joydev, const char *name);
joy_button_t *joy_button_from_code(joy_device_t *joydev, uint16_t code);
joy_button_t *joy_button_from_name(joy_device_t *joydev, const char *name);
joy_hat_t    *joy_hat_from_code   (joy_device_t *joydev, uint16_t code);
joy_hat_t    *joy_hat_from_name   (joy_device_t *joydev, const char *name);

joystick_axis_value_t joy_axis_value_from_hwdata(joy_axis_t *axis, int32_t hw_value);

void          joy_axis_event  (joy_device_t *joydev, joy_axis_t *axis, joystick_axis_value_t value);
void          joy_button_event(joy_device_t *joydev, joy_button_t *button, int32_t value);
void          joy_hat_event   (joy_device_t *joydev, joy_hat_t *hat, int32_t value);

bool          joy_open (joy_device_t *joydev);
bool          joy_poll (joy_device_t *joydev);
void          joy_close(joy_device_t *joydev);

#endif
