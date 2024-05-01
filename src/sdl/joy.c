/** \brief  joy.c
 * \brief   SDL joystick interface
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "joyapi.h"
#include "lib.h"

extern bool debug;
extern bool verbose;



/* minimal code to make the main program link */

bool joy_arch_init(void)
{
    printf("%s(): stub\n", __func__);
    return true;
}

int joy_arch_device_list_init(joy_device_t ***devices)
{
    printf("%s(): stub\n", __func__);
    *devices = NULL;
    return 0;
}

bool joy_arch_device_create_default_mapping(joy_device_t *joydev)
{
    (void)(joydev);
    printf("%s(): stub\n", __func__);
    return true;
}
