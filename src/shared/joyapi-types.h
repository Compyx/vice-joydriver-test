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

/** \brief  Host joystick input types */
typedef enum joy_input_e {
    JOY_INPUT_INVALID = -1,     /**< invalid */
    JOY_INPUT_AXIS,             /**< axis */
    JOY_INPUT_BUTTON,           /**< button */
    JOY_INPUT_HAT               /**< hat */
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

/** \brief  Calibration data for a mapping
 */
typedef struct joy_calibration_s {
    int32_t deadzone;   /**< deadzone (for non-digital inputs) */
    int32_t fuzz;       /**< fuzz for input values */
    int32_t threshold;  /**< cutoff for range to digital conversion */
    bool    configured; /**< calibration is set and must be applied */
} joy_calibration_t;

/** \brief  Mapping of host input to emulator input or action */
typedef struct joy_mapping_s {
    joy_action_t       action;      /**< type of mapping */
    union {
        int            pin;         /**< pin number for JOY_ACTION_JOYSTICK */
        joy_pot_axis_t pot;         /**< pot for JOY_ACTION_POT_AXIS */
        joy_key_map_t  key;         /**< key for JOY_ACTION_KEYBOARD */
        int            ui_action;   /**< UI action for JOY_ACTION_UI_ACTION */
    } target;
    bool               inverted;    /**< input should be inverted */ 
    joy_calibration_t  calibration; /**< calibration data */
} joy_mapping_t;

/** \brief  Hat directions
 */
typedef enum {
    JOY_HAT_INVALID = -1,   /**< invalid */
    JOY_HAT_CENTERED = 0,   /**< centered */
    JOY_HAT_NORTH,          /**< up */
    JOY_HAT_NORTHEAST,      /**< up + right */
    JOY_HAT_EAST,           /**< right */
    JOY_HAT_SOUTHEAST,      /**< down + right */
    JOY_HAT_SOUTH,          /**< down */
    JOY_HAT_SOUTHWEST,      /**< down + left */
    JOY_HAT_WEST,           /**< left */
    JOY_HAT_NORTHWEST       /**< up + left */
} joy_hat_direction_t;

/** \brief  Number of hat directions */
#define JOY_HAT_NUM_DIRECTIONS  (JOY_HAT_NORTHWEST + 1)

/** \brief  Digital axis positions */
typedef enum joystick_axis_value_e {
    JOY_AXIS_NEGATIVE = -1,     /**< negative direction (usually up/left) */
    JOY_AXIS_CENTERED =  0,     /**< centered */
    JOY_AXIS_POSITIVE =  1      /**< positive direction (usually down/right) */
} joystick_axis_value_t;

/* Emulated device directions and buttons */
#define JOYSTICK_DIRECTION_NONE        0    /**< emulated device direction none */
#define JOYSTICK_DIRECTION_UP          1    /**< emulated device direction up */
#define JOYSTICK_DIRECTION_DOWN        2    /**< emulated device direction down */
#define JOYSTICK_DIRECTION_LEFT        4    /**< emulated device direction left */
#define JOYSTICK_DIRECTION_RIGHT       8    /**< emulated device direction right */
#define JOYSTICK_BUTTON_FIRE1         16    /**< emulated device first fire button */
#define JOYSTICK_BUTTON_SNES_A        16    /**< SNES pad A button */
#define JOYSTICK_BUTTON_FIRE2         32    /**< emulated device second fire button */
#define JOYSTICK_BUTTON_SNES_B        32    /**< SNES pad B buton */
#define JOYSTICK_BUTTON_FIRE3         64    /**< emulated device third fire button */
#define JOYSTICK_BUTTON_SNES_X        64    /**< SNES pad X button */
#define JOYSTICK_BUTTON_SNES_Y       128    /**< SNES pad Y button */
#define JOYSTICK_BUTTON_SNES_L       256    /**< SNES pad left shoulder button */
#define JOYSTICK_BUTTON_SNES_R       512    /**< SNES pad right shoulder button */
#define JOYSTICK_BUTTON_SNES_SELECT 1024    /**< SNES pad Select button */
#define JOYSTICK_BUTTON_SNES_START  2048    /**< SNES pad Start button */

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
    struct {
        joy_mapping_t negative;     /**< axis negative direction pin mapping */
        joy_mapping_t positive;     /**< axis positive direction pin mapping */
        joy_mapping_t pot;          /**< axis to POT mapping */
    } mapping;                  /**< mappings */
} joy_axis_t;

/** \brief  Joystick hat object
 *
 * If \c code > 0 we use the hat map instead of the axes.
 */
typedef struct joy_hat_s {
    uint16_t             code;      /**< code in case of USB hat switch (BSD) */
    char                *name;      /**< name */
    int32_t              prev;      /**< previous value */
    joy_hat_direction_t  hat_map[JOY_HAT_NUM_DIRECTIONS];   /* hat mapping */
    struct {
        joy_mapping_t up;       /**< up direction of hat */
        joy_mapping_t down;     /**< down direction of hat */
        joy_mapping_t left;     /**< left direction of hat */
        joy_mapping_t right;    /**< right direction of hat */
    } mapping;                      /**< mappings of four hat directions */
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

    void         *hwdata;           /**< used for driver/arch-specific data */
} joy_device_t;

/** \brief  Joystick driver registration object
 */
typedef struct joy_driver_s {
    bool (*open)       (joy_device_t *joydev);  /**< open device for polling */
    bool (*poll)       (joy_device_t *joydev);  /**< poll device */
    void (*close)      (joy_device_t *joydev);  /**< close device */
    void (*hwdata_free)(void         *hwdata);  /**< free hardware-specific data */
} joy_driver_t;

/** \brief  Joymap file object
 */
typedef struct joymap_s {
    joy_device_t  *joydev;          /**< associated joystick device */
    char          *path;            /**< full path to joymap file */
    int            ver_major;       /**< VJM major version number */
    int            ver_minor;       /**< VJM minor version number */
    char          *dev_name;        /**< joystick device name */
    uint16_t       dev_vendor;      /**< joystick device vendor ID */
    uint16_t       dev_product;     /**< joystick device product ID */
    uint16_t       dev_version;     /**< joystick device product version */
} joymap_t;

#endif
