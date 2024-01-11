#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DIRECTINPUT_VERSION 0X0800
#define INITGUID
#include <dinput.h>

#include "config.h"
#include "lib.h"
#include "joyapi.h"

/** \brief  Iterator object used for the button enumerator callback */
typedef struct {
    joy_device_t *joydev;   /**< current joystick device */
    joy_button_t *list;     /**< list of buttons */
    size_t        size;     /**< number of allocated elements in \c list */
    size_t        index;    /**< index in \c list */
} button_iter_t;

/* "01234567-0123-0123-0123-0123456789AB\0" */
#define GUIDSTR_BUFSIZE 37u


#define BUTTONS_INITIAL_SIZE    32


/** \brief  Global DirectInput8 interface handle */
static LPDIRECTINPUT8 dinput_handle = NULL;

/** \brief  device list (used during querying) */
static joy_device_t **device_list = NULL;
/** \brief  device list index (used during querying) */
static size_t         device_list_index = 0;
/** \brief  device list allocated size (used during querying) */
static size_t         device_list_size = 0;


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
static BOOL enumbuttons_callback(LPCDIDEVICEOBJECTINSTANCE ddoi, LPVOID pvref)
{
    button_iter_t *iter = pvref;
    joy_button_t  *button;

    if (iter->index >= iter->joydev->num_buttons) {
        fprintf(stderr, "%s(): error: WinAPI lied about the number of buttons.\n",
                __func__);
        return DIENUM_STOP;
    }

    if (iter->index == iter->size) {
        iter->size *= 2u;
        iter->list  = lib_realloc(iter->list, iter->size * sizeof *(iter->list));
    }
    button       = &(iter->list[iter->index++]);
    button->code = DIDFT_GETINSTANCE(ddoi->dwType);
    button->name = lib_strdup(ddoi->tszName);

    return DIENUM_CONTINUE;
}


/** \brief  Callback for directintput8::EnumDevices()
 *
 * \param[in]   ddi     DirectInput device instance
 * \param[in]   pvref   window handle
 *
 * \return  \c DIENUM_CONTINUE
 */
static BOOL enumdevices_callback(LPCDIDEVICEINSTANCE ddi, LPVOID pvref)
{
    joy_device_t         *joydev;
    DIDEVCAPS             caps;
    LPDIRECTINPUTDEVICE8  didev;
    uint16_t              vendor;
    uint16_t              product;
    char                  instance_str[GUIDSTR_BUFSIZE];
    HRESULT               result;

    button_iter_t         button_iter;

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
                                            (HWND)pvref,
                                            DISCL_NONEXCLUSIVE|DISCL_BACKGROUND);
    caps.dwSize = sizeof(DIDEVCAPS);
    result = IDirectInputDevice8_GetCapabilities(didev, &caps);
    if (result != DI_OK) {
        return -1;
    }

    vendor  = (uint16_t)(ddi->guidProduct.Data1 & 0xffff);
    product = (uint16_t)((ddi->guidProduct.Data1 >> 16u) & 0xffff);
    guid_to_string(ddi->guidInstance, instance_str);

    if (device_list_index == device_list_size - 1u) {
        device_list_size *= 2u;
        device_list       = lib_realloc(device_list,
                            device_list_size * sizeof *device_list);
    }
    joydev = joy_device_new();
    joydev->name        = lib_strdup(ddi->tszProductName);
    joydev->node        = lib_strdup(instance_str);
    joydev->vendor      = vendor;
    joydev->product     = product;
    joydev->num_buttons = caps.dwButtons;
    joydev->num_axes    = caps.dwAxes;
    joydev->num_hats    = caps.dwPOVs;

    /* scan buttons */
    button_iter.joydev = joydev;
    button_iter.size   = BUTTONS_INITIAL_SIZE;
    button_iter.list   = lib_malloc(joydev->num_buttons * sizeof *(button_iter.list));
    button_iter.index  = 0;
    IDirectInputDevice8_EnumObjects(didev,
                                    enumbuttons_callback,
                                    (LPVOID)&button_iter,
                                    DIDFT_BUTTON);
    joydev->buttons = button_iter.list;

    device_list[device_list_index + 0u] = joydev;
    device_list[device_list_index + 1u] = NULL;
    device_list_index++;

    IDirectInputDevice8_Release(didev);
    return DIENUM_CONTINUE;
}


int joy_get_devices(joy_device_t ***devices)
{
    HRESULT   result;
    HINSTANCE window_handle;

    window_handle = GetModuleHandle(NULL);

    result = DirectInput8Create(window_handle,
                                DIRECTINPUT_VERSION,
                                &IID_IDirectInput8,
                                (void*)&dinput_handle,
                                NULL);
    if (result != 0) {
        printf("%s(): DirectInput8Create() failed: %ld\n", __func__, result);
        return -1;
    }

    device_list_index = 0;
    device_list_size  = 32u;
    device_list       = lib_malloc(device_list_size * sizeof *device_list);
    device_list[0]    = NULL;

    result = IDirectInput8_EnumDevices(dinput_handle,
                                       DIDEVTYPE_JOYSTICK,
                                       enumdevices_callback,
                                       window_handle,
                                       DIEDFL_ALLDEVICES);
    if (result != 0) {
        return -1;
    }

    *devices = device_list;
    return (int)device_list_index;
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
