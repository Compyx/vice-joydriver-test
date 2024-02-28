/** \file   joyapi-types.h
 * \brief   Types used by the joystick API
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef VICE_JOYAPI_TYPES_H
#define VICE_JOYAPI_TYPES_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Joystick mapping structs and enums
 */

/** \brief  Device cannot map to anything and needs to be rejected */
#define JOY_CAPS_NONE       0x00

/** \brief  Device is capable of mapping to paddle input
 *
 * For paddle emulation at least one axis and one button needs to be present.
 */
#define JOY_CAPS_PADDLE     0x01

/** \brief  Device is capable of mapping to mouse input
 *
 * For mouse emulation at least two axes and two buttons need to present.
 */
#define JOY_CAPS_MOUSE      0x02

/** \brief  Device is capable of mapping to Koala Pad input */
#define JOY_CAPS_KOALA      0x04

/** \brief  Device is capable of mapping to joystick input
 *
 * For joystick emulation at least two axes and one button, or a hat and one
 * button need to be present.
 */
#define JOY_CAPS_JOYSTICK   0x08

/** \brief  Types of mapping actions  */
typedef enum joy_action_e {
    JOY_ACTION_NONE,            /**< no mapping (ignore input) */
    JOY_ACTION_JOYSTICK,        /**< joystick pin */
    JOY_ACTION_KEYBOARD,        /**< key stroke */
    JOY_ACTION_POT_AXIS,        /**< pot axis */
    JOY_ACTION_UI_ACTION,       /**< trigger UI action */
    JOY_ACTION_UI_ACTIVATE      /**< activate menu (SDL) or settings dialog (Gtk3) */
} joy_action_t;

typedef enum joy_input_e {
    JOY_INPUT_INVALID = -1,
    JOY_INPUT_AXIS,
    JOY_INPUT_BUTTON,
    JOY_INPUT_HAT
} joy_input_t ;

/** \brief  Mapping of host input to emulated keyboard */
typedef struct joy_key_map_s {
    int          row;           /**< keyboard matrix row */
    int          column;        /**< keyboard matrix column */
    unsigned int flags;         /**< flags */
} joy_key_map_t;

/** \brief  Pot meter axes */
typedef enum joy_pot_axis_s {
    JOY_POTX,       /**< POTX register */
    JOY_POTY        /**< POTY register */
} joy_pot_axis_t;

#define JOY_AXIS_IDX_NEG    0   /**< negative axis index */
#define JOY_AXIS_IDX_POS    1   /**< positive axis index */
#define JOY_AXIS_IDX_NUM    2   /**< number of axis indexes */

/** \brief  Calibration data for a "normal" axis
 *
 * Calibration data for an axis that has a neutral position in the middle of
 * its range. For triggers we might need a different data structure.
 */
typedef struct joy_calibration_s {
    int32_t threshold_neg;
    int32_t threshold_pos;
    int32_t deadzone_neg;
    int32_t deadzone_pos;
    bool    invert;
} joy_calibration_t;

/** \brief  Mapping of host input to emulator input or action */
typedef struct joy_mapping_s {
    joy_action_t       action;
    union {
        int            pin;         /* JOY_ACTION_JOYSTICK */
        joy_pot_axis_t pot;         /* JOY_ACTION_POT_AXIS */
        joy_key_map_t  key;         /* JOY_ACTION_KEYBOARD */
        int            ui_action;   /* JOY_ACTION_UI_ACTION */
    } target;
    joy_calibration_t  calibration;
} joy_mapping_t;


typedef enum {
    JOY_HAT_INVALID = -1,
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

typedef enum joystick_axis_value_e {
    JOY_AXIS_NEGATIVE = -1,
    JOY_AXIS_MIDDLE   =  0,
    JOY_AXIS_POSITIVE =  1
} joystick_axis_value_t;


#define JOYSTICK_DIRECTION_NONE     0
#define JOYSTICK_DIRECTION_UP       1
#define JOYSTICK_DIRECTION_DOWN     2
#define JOYSTICK_DIRECTION_LEFT     4
#define JOYSTICK_DIRECTION_RIGHT    8


#define JOY_HAT_NUM_DIRECTIONS  (JOY_HAT_NORTHWEST + 1)

/** \brief  Joystick button object */
typedef struct joy_button_s {
    uint16_t       code;        /**< event code */
    char          *name;        /**< name */
    int32_t        prev;        /**< previous value */
    joy_mapping_t  mapping;     /**< input mapping */
} joy_button_t;

/** \brief  Joystick axis object */
typedef struct joy_axis_s {
    uint16_t       code;        /**< event code */
    char          *name;        /**< name */
    int32_t        prev;        /**< previous value */
    int32_t        minimum;     /**< minimum value */
    int32_t        maximum;     /**< maximum value */
    int32_t        fuzz;        /**< noise removed by dev driver (Linux) */
    int32_t        flat;        /**< flat (Linux only) */
    int32_t        resolution;  /**< resolution of axis (units per mm) */
    uint32_t       granularity; /**< granularity of reported values (Windows) */
    bool           digital;     /**< axis is digital */
    union {
        joy_mapping_t pin[JOY_AXIS_IDX_NUM];
        joy_mapping_t pot;
    } mapping;
} joy_axis_t;

/** \brief  Joystick hat object
 *
 * If \c code > 0 we use the hat map instead of the axes.
 */
typedef struct joy_hat_s {
    uint16_t             code;      /**< code in case of USB hat switch (BSD) */
    char                *name;      /**< name */
    int32_t              prev;      /**< previous value (Windows) */
    joy_axis_t           x;         /**< X axis */
    joy_axis_t           y;         /**< Y axis */
    joy_hat_direction_t  hat_map[JOY_HAT_NUM_DIRECTIONS];   /* hat mapping */
    joy_mapping_t        mapping;   /**< mapping */
} joy_hat_t;

/** \brief  Joystick device object */
typedef struct joy_device_s {
    char         *name;             /**< name */
    char         *node;             /**< device node (Unix) or instance GUID
                                         (Windows) */
    uint16_t      vendor;           /**< vendor ID */
    uint16_t      product;          /**< product ID */
    uint16_t      version;          /**< version number */

    uint32_t      num_buttons;      /**< number of buttons */
    uint32_t      num_axes;         /**< number of axes */
    uint32_t      num_hats;         /**< number of hats */

    joy_button_t *buttons;          /**< list of buttons */
    joy_axis_t   *axes;             /**< list of axes */
    joy_hat_t    *hats;             /**< list of hats */

    int           port;             /**< port number (0-based, -1 = unassigned) */
    uint32_t      capabilities;     /**< capabilities bitmask */

    void         *priv;             /**< used for driver/arch-specific data */
} joy_device_t;

/** \brief  Joystick driver registration object
 */
typedef struct joy_driver_s {
    bool (*open)     (joy_device_t *joydev);    /**< open device for polling */
    bool (*poll)     (joy_device_t *joydev);    /**< poll device */
    void (*close)    (joy_device_t *joydev);    /**< close device */
    void (*priv_free)(void         *priv);      /**< free private data */
} joy_driver_t;

typedef struct joymap_s {
    joy_device_t  *joydev;
    char          *path;
    FILE          *fp;
    int            ver_major;
    int            ver_minor;
    char          *dev_name;
    uint16_t       dev_vendor;
    uint16_t       dev_product;
    uint16_t       dev_version;
    joy_mapping_t *mappings;
    uint32_t       mappings_size;
    uint32_t       mappings_num;
} joymap_t;

#endif
