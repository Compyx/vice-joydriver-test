#include <stdio.h>
#include <stdlib.h>
#define DIRECTINPUT_VERSION 0X0800
#define INITGUID
#include <dinput.h>

#include "config.h"
#include "lib.h"
#include "joyapi.h"


static LPDIRECTINPUT8 dinput_handle = NULL;
static int num_devices = 0;


/** \brief  Callback for directintput8::EnumDevices()
 *
 * \param[in]   ddi     DirectInput handle
 * \param[in]   pvref   window handle
 *
 * \return  \c DIENUM_CONTINUE
 */
static BOOL enumdevices_callback(LPCDIDEVICEINSTANCE ddi, LPVOID pvref)
{
    uint16_t vendor;
    uint16_t product;

    vendor  = (uint16_t)(ddi->guidProduct.Data1 & 0xffff);
    product = (uint16_t)((ddi->guidProduct.Data1 >> 16u) & 0xffff);

    printf("%s(): name   : %s\n", __func__, ddi->tszProductName);
    printf("%s(): vendor : %04x\n", __func__, (unsigned int)(vendor));
    printf("%s(): product: %04x\n", __func__, (unsigned int)(product));
    printf("----\n");
    num_devices++;

    return DIENUM_CONTINUE;
}


int joy_get_devices(joy_device_t ***devices)
{
    HRESULT   result;
    HINSTANCE window_handle;

    window_handle = GetModuleHandle(NULL);
    num_devices   = 0;

    result = DirectInput8Create(window_handle,
                                DIRECTINPUT_VERSION,
                                &IID_IDirectInput8,
                                (void*)&dinput_handle,
                                NULL);
    if (result != 0) {
        printf("%s(): DirectInput8Create() failed: %ld\n", __func__, result);
        return -1;
    }

    result = IDirectInput8_EnumDevices(dinput_handle,
                                       DIDEVTYPE_JOYSTICK,
                                       enumdevices_callback,
                                       window_handle,
                                       DIEDFL_ALLDEVICES);
    if (result != 0) {
        return -1;
    }

    *devices = NULL;
    return num_devices;
}
