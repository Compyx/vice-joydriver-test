/** \file   joyapi.h
 * \brief   Joystick interface API
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef JOYAPI_H
#define JOYAPI_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    JOY_HAT_NEUTRAL = 0,
    JOY_HAT_NORTH,
    JOY_HAT_NORTHEAST,
    JOY_HAT_EAST,
    JOY_HAT_SOUTHEAST,
    JOY_HAT_SOUTH,
    JOY_HAT_SOUTHWEST,
    JOY_HAT_WEST,
    JOY_HAT_NORTHWEST
} joy_hat_direction_t;

#define JOY_HAT_NUM_DIRECTIONS  (JOY_HAT_NORTHWEST + 1)


/** \brief  Joystick button object */
typedef struct joy_button_s {
    uint16_t  code;     /**< event code */
    char     *name;     /**< name */
} joy_button_t;

/** \brief  Joystick axis object */
typedef struct joy_axis_s {
    uint16_t  code;             /**< event code */
    char     *name;             /**< name */
    int32_t   minimum;          /**< minimum value */
    int32_t   maximum;          /**< maximum value */
    int32_t   fuzz;             /**< noise removed by dev driver (Linux) */
    int32_t   flat;             /**< flat (Linux only) */
    int32_t   resolution;       /**< resolution of axis (units per mm) */
    uint32_t  granularity;      /**< granularity of reported values (Windows) */
} joy_axis_t;

/** \brief  Joystick hat object
 *
 * If \c code > 0 we use the hat map instead of the axes.
 */
typedef struct joy_hat_s {
    uint16_t             code;  /**< code in case of USB hat switch (BSD) */
    char                *name;  /**< name */
    joy_axis_t           x;     /**< X axis */
    joy_axis_t           y;     /**< Y axis */
    joy_hat_direction_t  hat_map[JOY_HAT_NUM_DIRECTIONS];   /* hat mapping */
} joy_hat_t;

/** \brief  Joystick device object */
typedef struct joy_device_s {
    char         *name;             /**< name */
    char         *node;             /**< device node (Unix) or instance GUID
                                         (Windows) */
    uint16_t      vendor;           /**< vendor ID */
    uint16_t      product;          /**< product ID */

    uint32_t      num_buttons;      /**< number of buttons */
    uint32_t      num_axes;         /**< number of axes */
    uint32_t      num_hats;         /**< number of hats */

    joy_button_t *buttons;          /**< list of buttons */
    joy_axis_t   *axes;             /**< list of axes */
    joy_hat_t    *hats;             /**< list of hats */

    void         *priv;             /**< UNUSED so far, can be used for driver-
                                         or OS-specific data if absolutely
                                         required */
} joy_device_t;

/** \brief  Joystick driver registration object
 */
typedef struct joy_driver_s {
    bool (*open) (joy_device_t *joydev); /**< open device for polling */
    bool (*poll) (joy_device_t *joydev); /**< poll device */
    void (*close)(joy_device_t *joydev); /**< close device */
} joy_driver_t;


/*
 * Prototypes mark 'arch' are expected to be implemented for the arch using
 * arch-specific code.
 * TODO:    Perhaps affix these with _arch_ like the hotkeys API does to make
 *          it clear which functions are expected to be implemented for an
 *          arch and which are provided by VICE independent of arch.
 */

bool          joy_init(void);   /* arch */

void          joy_driver_register(const joy_driver_t *drv);
int           joy_device_list_init(joy_device_t ***devices);    /* arch */

void          joy_device_list_free(joy_device_t  **devices);

joy_device_t *joy_device_new (void);
void          joy_device_free(joy_device_t *dev);
void          joy_device_dump(const joy_device_t *dev, bool verbose);
joy_device_t *joy_device_get(joy_device_t **devices, const char *node);

const char   *joy_device_get_button_name(const joy_device_t *joydev, uint16_t code);
const char   *joy_device_get_axis_name  (const joy_device_t *joydev, uint16_t code);
const char   *joy_device_get_hat_name   (const joy_device_t *joydev, uint16_t code);

void          joy_axis_init  (joy_axis_t   *axis);
void          joy_button_init(joy_button_t *button);
void          joy_hat_init   (joy_hat_t    *hat);


void          joy_axis_event  (const joy_device_t *joydev, uint16_t code, int32_t value);
void          joy_button_event(const joy_device_t *joydev, uint16_t code, int32_t value);
void          joy_hat_event   (const joy_device_t *joydev, uint16_t code, int32_t value);

bool          joy_open (joy_device_t *joydev);
bool          joy_poll (joy_device_t *joydev);
void          joy_close(joy_device_t *joydev);

#endif
