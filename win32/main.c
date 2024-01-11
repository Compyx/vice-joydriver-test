#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

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
    if (result > 0) {
        for (int i = 0; i < result; i++) {
            joy_device_dump(devices[i], true);
        }
    }
    joy_free_devices(devices);
    return EXIT_SUCCESS;
}
