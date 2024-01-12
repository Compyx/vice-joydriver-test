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


/** \brief  Global DirectInput8 interface handle */
static LPDIRECTINPUT8 dinput_handle = NULL;


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
        fprintf(stderr, "%s(): error: WinAPI lied about the number of buttons.\n",
                __func__);
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
    printf("axis %u: %s\n", axis->code, axis->name);

    /* get logical range */
    range.diph.dwSize       = sizeof(DIPROPRANGE);
    range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    range.diph.dwObj        = ddoi->dwType;
    range.diph.dwHow        = DIPH_BYID;
    result = IDirectInputDevice8_GetProperty(iter->didev,
                                             DIPROP_LOGICALRANGE,
                                             &range.diph);
    if (SUCCEEDED(result)) {
        printf("range: %ld - %ld\n", range.lMin, range.lMax);
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
        printf("granularity: %lu\n", granularity.dwData);
        axis->granularity = granularity.dwData;
    }

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

    window = GetModuleHandle(NULL);
    device_iter = pvref;


    /* get capabilities of device */
    result = IDirectInput8_CreateDevice(dinput_handle,
                                        &(ddi->guidInstance),
                                        &didev,
                                        NULL);
    if (result != DI_OK) {
        return -1;
    }
    IDirectInputDevice8_SetDataFormat(didev, &c_dfDIJoystick);
    IDirectInputDevice8_SetCooperativeLevel(didev,
                                            (HWND)window,
                                            DISCL_NONEXCLUSIVE|DISCL_BACKGROUND);
    caps.dwSize = sizeof(DIDEVCAPS);
    result = IDirectInputDevice8_GetCapabilities(didev, &caps);
    if (result != DI_OK) {
        return -1;
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


    device_iter->list[device_iter->index++] = joydev;
    device_iter->list[device_iter->index]   = NULL;

    IDirectInputDevice8_Release(didev);
    return DIENUM_CONTINUE;
}


int joy_get_devices(joy_device_t ***devices)
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
        printf("%s(): DirectInput8Create() failed: %ld\n", __func__, result);
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


void joy_free_devices(joy_device_t **devices)
{
    if (devices != NULL) {
        for (size_t i = 0; devices[i] != NULL; i++) {
            joy_device_free(devices[i]);
        }
        lib_free(devices);
    }
}
