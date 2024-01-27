/** \file   joyapi.h
 * \brief   Joystick interface API
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef JOYAPI_H
#define JOYAPI_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Joystick mapping structs and enums
 */

/** \brief  Types of mapping actions  */
typedef enum joy_action_s {
    JOY_ACTION_NONE,            /**< no mapping (ignore input) */
    JOY_ACTION_JOYSTICK,        /**< joystick pin */
    JOY_ACTION_KEYBOARD,        /**< key stroke */
    JOY_ACTION_POT_AXIS,        /**< pot axis */
    JOY_ACTION_UI_ACTION,       /**< trigger UI action */
    JOY_ACTION_UI_ACTIVATE      /**< activate menu (SDL) or settings dialog (Gtk3) */
} joy_action_t;

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

/** \brief  Mapping of host input to emulator input or action */
typedef struct joy_mapping_s {
    joy_action_t       action;
    union {
        int            pin;         /* JOY_ACTION_JOYSTICK */
        joy_pot_axis_t pot;         /* JOY_ACTION_POT_AXIS */
        joy_key_map_t  key;         /* JOY_ACTION_KEYBOARD */
        int            ui_action;   /* JOY_ACTION_UI_ACTION */
    } data;
} joy_mapping_t;


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
    joy_mapping_t  mapping;     /**< input mapping */
} joy_axis_t;

/** \brief  Joystick hat object
 *
 * If \c code > 0 we use the hat map instead of the axes.
 */
typedef struct joy_hat_s {
    uint16_t             code;      /**< code in case of USB hat switch (BSD) */
    char                *name;      /**< name */
    int32_t              prev;      /**< previous value */
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

    uint32_t      num_buttons;      /**< number of buttons */
    uint32_t      num_axes;         /**< number of axes */
    uint32_t      num_hats;         /**< number of hats */

    joy_button_t *buttons;          /**< list of buttons */
    joy_axis_t   *axes;             /**< list of axes */
    joy_hat_t    *hats;             /**< list of hats */

    int           port;             /**< port number (0-based, -1 = unassigned) */

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
void          joy_device_dump(const joy_device_t *dev);
joy_device_t *joy_device_get(joy_device_t **devices, const char *node);

const char   *joy_device_get_button_name(const joy_device_t *joydev, uint16_t code);
const char   *joy_device_get_axis_name  (const joy_device_t *joydev, uint16_t code);
const char   *joy_device_get_hat_name   (const joy_device_t *joydev, uint16_t code);

void          joy_mapping_init(joy_mapping_t *mapping);
void          joy_axis_init   (joy_axis_t    *axis);
void          joy_button_init (joy_button_t  *button);
void          joy_hat_init    (joy_hat_t     *hat);

joy_axis_t   *joy_axis_from_code  (joy_device_t *joydev, uint16_t code);
joy_button_t *joy_button_from_code(joy_device_t *joydev, uint16_t code);
joy_hat_t    *joy_hat_from_code   (joy_device_t *joydev, uint16_t code);

void          joy_axis_event  (joy_device_t *joydev, joy_axis_t   *axis,   int32_t value);
void          joy_button_event(joy_device_t *joydev, joy_button_t *button, int32_t value);
void          joy_hat_event   (joy_device_t *joydev, joy_hat_t    *hat,    int32_t value);

bool          joy_open (joy_device_t *joydev);
bool          joy_poll (joy_device_t *joydev);
void          joy_close(joy_device_t *joydev);

#endif
