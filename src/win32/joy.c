#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DIRECTINPUT_VERSION 0X0800
#define INITGUID
#include <dinput.h>

#include "config.h"
#include "lib.h"
#include "joyapi.h"


/** \brief  Iterator used for the devices enumerator callback */
typedef struct {
    joy_device_t **list;    /**< list of devices */
    size_t         size;    /**< number of allocated elements in \c list */
    size_t         index;   /**< index in \c list */
} device_iter_t;

/** \brief  Iterator used for the button enumerator callback */
typedef struct {
    joy_device_t *joydev;   /**< current joystick device */
    joy_button_t *list;     /**< list of buttons */
    size_t        size;     /**< number of allocated elements in \c list */
    size_t        index;    /**< index in \c list */
} button_iter_t;

/** \brief  Iterator used for the axis enumerator callback */
typedef struct {
    joy_device_t         *joydev;
    LPDIRECTINPUTDEVICE8  didev;    /**< DirectInput device instance, required
                                         to obtain properties like range */
    joy_axis_t           *list;     /**< list of axes */
    size_t                size;     /**< number of allocated elements in \a list */
    size_t                index;    /**< index in \a list */
} axis_iter_t;

/** \brief  Iterator used for the hat (POV) enumator callback */
typedef struct {
    joy_device_t         *joydev;
    LPDIRECTINPUTDEVICE8  didev;
    joy_hat_t            *list;
    size_t                size;
    size_t                index;
} hat_iter_t;


/** \brief  Initialize EnumDevices callback iterator
 *
 * \param[in]   iter    iterator
 * \param[in]   _size   number of elements to allocate
 */
#define INIT_LIST_ITER(iter, _size) \
    iter.size  = _size; \
    iter.index = 0; \
    iter.list  = lib_malloc(_size * sizeof *(iter.list));

/** \brief  Resize list of iterator if required
 *
 * If the list in \a iter_ptr is full we double its size.
 *
 * \param[in]   iter_ptr    pointer to iterator
 */
#define RESIZE_LIST_ITER(iter_ptr) \
    if (iter_ptr->index == iter_ptr->size) { \
        iter_ptr->size *= 2u; \
        iter_ptr->list  = lib_realloc(iter_ptr->list, \
                                      iter_ptr->size * sizeof *(iter_ptr->list)); \
    }


/* "01234567-0123-0123-0123-0123456789AB\0" */
#define GUIDSTR_BUFSIZE 37u

/** \brief  Initial number of elements allocated for the buttons list */
#define BUTTONS_INITIAL_SIZE    32

/** \brief  Initial number of elements allocated for the axes list */
#define AXES_INITIAL_SIZE   16

/** \brief  Initial number of elements allocated for the hats list */
#define HATS_INITIAL_SIZE   4


typedef struct hwdata_s {
    LPDIRECTINPUTDEVICE8 didev;
} hwdata_t;


/** \brief  Global DirectInput8 interface handle */
static LPDIRECTINPUT8 dinput_handle = NULL;



static hwdata_t *hwdata_new(void)
{
    hwdata_t *hwdata = lib_malloc(sizeof *hwdata);

    hwdata->didev = NULL;
    return hwdata;
}

static void hwdata_free(void *hwdata)
{
    hwdata_t *hw = hwdata;

    if (hw != NULL) {
        IDirectInputDevice8_Release(hw->didev);
        lib_free(hw);
    }
}



/** \brief  Transform a WinAPI "GUID" to string
 *
 * \param[in]   guid    GUID instance
 * \param[out]  buffer  buffer to store string
 *
 * \note    \a buffer must be at least \c GUIDSTR_BUFSIZE (37) bytes.
 */
static void guid_to_string(GUID guid, char *buffer)
{
    snprintf(buffer, GUIDSTR_BUFSIZE,
            "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            guid.Data1,
            guid.Data2,
            guid.Data3,
            guid.Data4[0], guid.Data4[1],
            guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    buffer[GUIDSTR_BUFSIZE - 1U] = '\0';

}

/** \brief  Callback for directintput8::EnumDevices()
 *
 * \param[in]   ddi     DirectInput device object instance
 * \param[in]   pvref   button iterator
 *
 * \return  \c DIENUM_CONTINUE or \c DIEENUM_STOP on error
 */
static BOOL EnumObjects_buttons_cb(LPCDIDEVICEOBJECTINSTANCE ddoi, LPVOID pvref)
{
    button_iter_t *iter = pvref;
    joy_button_t  *button;

    if (iter->index >= iter->joydev->num_buttons) {
        msg_error("WinAPI lied about the number of buttons.\n");
        return DIENUM_STOP;
    }

    RESIZE_LIST_ITER(iter);

    button = &(iter->list[iter->index++]);
    joy_button_init(button);
    button->code = DIDFT_GETINSTANCE(ddoi->dwType);
    button->name = lib_strdup(ddoi->tszName);

    return DIENUM_CONTINUE;
}

static BOOL EnumObjects_axes_cb(LPCDIDEVICEOBJECTINSTANCE ddoi, LPVOID pvref)
{
    axis_iter_t *iter = pvref;
    joy_axis_t  *axis;
    HRESULT      result;
    DIPROPRANGE  range;
    DIPROPDWORD  granularity;

    RESIZE_LIST_ITER(iter);

    axis = &(iter->list[iter->index++]);
    joy_axis_init(axis);
    axis->code = DIDFT_GETINSTANCE(ddoi->dwType);
    axis->name = lib_strdup(ddoi->tszName);
    msg_debug("axis %u: %s\n", axis->code, axis->name);

    /* get logical range */
    range.diph.dwSize       = sizeof(DIPROPRANGE);
    range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    range.diph.dwObj        = ddoi->dwType;
    range.diph.dwHow        = DIPH_BYID;
    result = IDirectInputDevice8_GetProperty(iter->didev,
                                             DIPROP_LOGICALRANGE,
                                             &range.diph);
    if (SUCCEEDED(result)) {
        msg_debug("range: %ld - %ld\n", range.lMin, range.lMax);
        axis->minimum = range.lMin;
        axis->maximum = range.lMax;
    }

    /* get granularity */
    granularity.diph.dwSize       = sizeof(DIPROPDWORD);
    granularity.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    granularity.diph.dwObj        = ddoi->dwType;
    granularity.diph.dwHow        = DIPH_BYID;
    result = IDirectInputDevice8_GetProperty(iter->didev,
                                             DIPROP_GRANULARITY,
                                             &granularity.diph);
    if (SUCCEEDED(result)) {
//        printf("granularity: %lu\n", granularity.dwData);
        axis->granularity = granularity.dwData;
    }

    return DIENUM_CONTINUE;
}

static BOOL EnumObjects_hats_cb(LPCDIDEVICEOBJECTINSTANCE ddoi, LPVOID pvref)
{
    hat_iter_t *iter = pvref;
    joy_hat_t  *hat;

    RESIZE_LIST_ITER(iter);

    hat = &(iter->list[iter->index++]);

    hat->name = lib_strdup(ddoi->tszName);
    msg_debug("hat name = %s\n", hat->name);

    return DIENUM_CONTINUE;
}

/** \brief  Callback for directintput8::EnumDevices()
 *
 * \param[in]   ddi     DirectInput device instance
 * \param[in]   pvref   window handle
 *
 * \return  \c DIENUM_CONTINUE
 */
static BOOL EnumDevices_cb(LPCDIDEVICEINSTANCE ddi, LPVOID pvref)
{
    joy_device_t         *joydev;
    hwdata_t             *hwdata;
    DIDEVCAPS             caps;
    LPDIRECTINPUTDEVICE8  didev;
    uint16_t              vendor;
    uint16_t              product;
    char                  instance_str[GUIDSTR_BUFSIZE];
    HRESULT               result;
    HINSTANCE             window;
    device_iter_t        *device_iter;
    button_iter_t         button_iter;
    axis_iter_t           axis_iter;
    hat_iter_t            hat_iter;

    window      = GetModuleHandle(NULL);
    device_iter = pvref;

    /* get capabilities of device */
    result = IDirectInput8_CreateDevice(dinput_handle,
                                        &(ddi->guidInstance),
                                        &didev,
                                        NULL);
    if (result != DI_OK) {
        return DIENUM_STOP;
    }
    IDirectInputDevice8_SetDataFormat(didev, &c_dfDIJoystick2);
    IDirectInputDevice8_SetCooperativeLevel(didev,
                                            (HWND)window,
                                            DISCL_NONEXCLUSIVE|DISCL_BACKGROUND);
    caps.dwSize = sizeof(DIDEVCAPS);
    result = IDirectInputDevice8_GetCapabilities(didev, &caps);
    if (result != DI_OK) {
        return DIENUM_STOP;
    }

    vendor  = (uint16_t)(ddi->guidProduct.Data1 & 0xffff);
    product = (uint16_t)((ddi->guidProduct.Data1 >> 16u) & 0xffff);
    guid_to_string(ddi->guidInstance, instance_str);

    /* resize list if required (-1 for the terminating NULL) */
    if (device_iter->index == device_iter->size - 1u) {
        device_iter->size *= 2u;
        device_iter->list  = lib_realloc(device_iter->list,
                                         device_iter->size * sizeof *(device_iter->list));
    }

    joydev              = joy_device_new();
    joydev->name        = lib_strdup(ddi->tszProductName);
    joydev->node        = lib_strdup(instance_str);
    joydev->vendor      = vendor;
    joydev->product     = product;
    joydev->num_buttons = caps.dwButtons;
    joydev->num_axes    = caps.dwAxes;
    joydev->num_hats    = caps.dwPOVs;

    /* scan buttons */
    INIT_LIST_ITER(button_iter, BUTTONS_INITIAL_SIZE);
    button_iter.joydev = joydev;
    IDirectInputDevice8_EnumObjects(didev,
                                    EnumObjects_buttons_cb,
                                    (LPVOID)&button_iter,
                                    DIDFT_BUTTON);
    joydev->buttons = button_iter.list;

    /* scan axes */
    INIT_LIST_ITER(axis_iter, AXES_INITIAL_SIZE);
    axis_iter.joydev = joydev;
    axis_iter.didev  = didev;
    IDirectInputDevice8_EnumObjects(didev,
                                    EnumObjects_axes_cb,
                                    (LPVOID)&axis_iter,
                                    DIDFT_ABSAXIS);
    joydev->axes = axis_iter.list;

    /* scan hats (POVs) */
    INIT_LIST_ITER(hat_iter, HATS_INITIAL_SIZE);
    hat_iter.joydev = joydev;
    hat_iter.didev  = didev;
    IDirectInputDevice8_EnumObjects(didev,
                                    EnumObjects_hats_cb,
                                    (LPVOID)&hat_iter,
                                    DIDFT_POV);
    joydev->hats = hat_iter.list;


    device_iter->list[device_iter->index++] = joydev;
    device_iter->list[device_iter->index]   = NULL;

    hwdata         = hwdata_new();
    hwdata->didev  = didev;
    joydev->hwdata = hwdata;

    return DIENUM_CONTINUE;
}


int joy_arch_device_list_init(joy_device_t ***devices)
{
    HRESULT       result;
    HINSTANCE     window;
    device_iter_t iter;

    window = GetModuleHandle(NULL);

    result = DirectInput8Create(window,
                                DIRECTINPUT_VERSION,
                                &IID_IDirectInput8,
                                (void*)&dinput_handle,
                                NULL);
    if (result != 0) {
        msg_error("DirectInput8Create() failed: %lx\n", result);
        return -1;
    }

    /* initialize iterator for use in the callback */
    iter.size    = 32u;
    iter.index   = 0;
    iter.list    = lib_malloc(iter.size * sizeof *(iter.list));
    iter.list[0] = NULL;

    result = IDirectInput8_EnumDevices(dinput_handle,
                                       DIDEVTYPE_JOYSTICK,
                                       EnumDevices_cb,
                                       (LPVOID)&iter,
                                       DIEDFL_ALLDEVICES);
    if (result != 0) {
        return -1;
    }

    *devices = iter.list;
    return (int)iter.index;
}


static bool joydev_open(joy_device_t *joydev)
{
    hwdata_t *hwdata = joydev->hwdata;

    msg_debug("opening device %s: ", joydev->name);
    if (IDirectInputDevice8_Acquire(hwdata->didev) != DI_OK) {
        if (debug) {
            printf("failed!\n");
        }
        return false;
    }
    if (debug) {
        printf("OK\n");
    }
    return true;
}


static bool joydev_poll(joy_device_t *joydev)
{
    hwdata_t             *hwdata;
    DIJOYSTATE2           jstate;
    LPDIRECTINPUTDEVICE8  didev;
    HRESULT               result;
    LONG                 *axis_values[] = {
        &jstate.lX,   &jstate.lY,   &jstate.lZ,
        &jstate.lRx,  &jstate.lRy,  &jstate.lRz,
        &jstate.lVX,  &jstate.lVY,  &jstate.lVZ,
        &jstate.lVRx, &jstate.lVRy, &jstate.lVRz,
        &jstate.lAX,  &jstate.lAY,  &jstate.lAZ,
        &jstate.lARx, &jstate.lARx, &jstate.lARz,
        &jstate.lFX,  &jstate.lFY,  &jstate.lFZ,
        &jstate.lFRx, &jstate.lFRy, &jstate.lFRz
    };


    hwdata = joydev->hwdata;
    didev  = hwdata->didev;
    result = IDirectInputDevice8_Poll(didev);
    if (result != DI_OK && result != DI_NOEFFECT) {
        msg_error("IDirectInputDevice8::Poll() failed: %lx\n", result);
        return false;
    }
    result = IDirectInputDevice8_GetDeviceState(didev, sizeof(DIJOYSTATE2), &jstate);
    if (result != DI_OK) {
        msg_error("IDirectInputDevice8::GetDeviceState() failed: %lx\n", result);
        return false;
    }

    /* button events */
    for (uint32_t b = 0; b < joydev->num_buttons && b < ARRAY_LEN(jstate.rgbButtons); b++) {
        /* no need to look up button via code, just use index */
        joy_button_t *button = &(joydev->buttons[b]);
        int32_t       newval = jstate.rgbButtons[b] & 0x80;

        /* trigger button event if the state changed */
        if (button->prev != newval) {
            joy_button_event(joydev, button, newval),
            button->prev = newval;
        }
    }

    /* axis events */
    for (uint32_t a = 0; a < joydev->num_axes && a < ARRAY_LEN(axis_values); a++) {
        joy_axis_t *axis   = &(joydev->axes[a]);
        LONG       *value  = axis_values[a];
        int32_t     newval = (int32_t)*value;

        if (newval != axis->prev) {
            joy_axis_event(joydev, axis, newval);
            axis->prev = newval;
        }
    }

    /* hat events */
    for (uint32_t h = 0; h < joydev->num_hats && h < ARRAY_LEN(jstate.rgdwPOV); h++) {
        joy_hat_t *hat       = &(joydev->hats[h]);
        int32_t    newval    = (int32_t)(jstate.rgdwPOV[h]);
        int32_t    direction = JOYSTICK_DIRECTION_NONE;

        if (newval != hat->prev) {
            hat->prev = newval;

            /* POVs map to 360 degrees, in units of 100th of a degree */
            /* -1 / 0xffffffff is neutral, also discard invalid values */
            if (newval < 0 || newval >= 36000) {
                direction = JOYSTICK_DIRECTION_NONE;
            } else if (newval >= 33750 || newval <  2250) {
                direction = JOYSTICK_DIRECTION_UP;
            } else if (newval >=  2250 && newval <  6750) {
                direction = JOYSTICK_DIRECTION_UP|JOYSTICK_DIRECTION_RIGHT;
            } else if (newval >=  6750 && newval < 11250) {
                direction = JOYSTICK_DIRECTION_RIGHT;
            } else if (newval >= 11250 && newval < 15750) {
                direction = JOYSTICK_DIRECTION_RIGHT|JOYSTICK_DIRECTION_DOWN;
            } else if (newval >= 15750 && newval < 20250) {
                direction = JOYSTICK_DIRECTION_DOWN;
            } else if (newval >= 20250 && newval < 24750) {
                direction = JOYSTICK_DIRECTION_DOWN|JOYSTICK_DIRECTION_LEFT;
            } else if (newval >= 24750 && newval < 29250) {
                direction = JOYSTICK_DIRECTION_LEFT;
            } else if (newval >= 29250 && newval < 33750) {
                direction = JOYSTICK_DIRECTION_LEFT|JOYSTICK_DIRECTION_UP;
            }

            /* TODO: what about releasing directions? */
            if (direction & JOYSTICK_DIRECTION_UP) {
                joy_hat_event(joydev, hat, &hat->mapping.up, direction);
            }
            if (direction & JOYSTICK_DIRECTION_DOWN) {
                joy_hat_event(joydev, hat, &hat->mapping.down, direction);
            }
            if (direction & JOYSTICK_DIRECTION_LEFT) {
                joy_hat_event(joydev, hat, &hat->mapping.left, direction);
            }
            if (direction & JOYSTICK_DIRECTION_RIGHT) {
                joy_hat_event(joydev, hat, &hat->mapping.right, direction);
            }
        }
    }


    return true;
}

static void joydev_close(joy_device_t *joydev)
{
    hwdata_t *hwdata = joydev->hwdata;

    if (hwdata != NULL) {
        IDirectInputDevice8_Unacquire(hwdata->didev);
    }
}



bool joy_arch_init(void)
{
    joy_driver_t driver = {
        .open        = joydev_open,
        .poll        = joydev_poll,
        .close       = joydev_close,
        .hwdata_free = hwdata_free
    };

    joy_driver_register(&driver);
    return true;
}


bool joy_arch_device_create_default_mapping(joy_device_t *joydev)
{
    if (joydev->num_buttons < 1u) {
        return false;
    }
    return true;
}
