#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "lib.h"
#include "joyapi.h"


int main(void)
{
    joy_device_t **devices = NULL;
    int            result;

    puts("Hello Windows :(");

    result = joy_get_devices(&devices);
    printf("result = %d\n", result);
    return EXIT_SUCCESS;
}
