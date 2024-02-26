/** \file   joymap.h
 * \brief   VICE joymap file (vjm) parsing - header
 *
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef JOYMAP_H
#define JOYMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VJM_VERSION_MAJOR   2
#define VJM_VERSION_MINOR   0


typedef struct joymap_s {
    char     *path;
    FILE     *fp;
    int       ver_major;
    int       ver_minor;
    char     *dev_name;
    uint16_t  dev_vendor;
    uint16_t  dev_product;
    uint16_t  dev_version;
} joymap_t;

void      joymap_module_init(void);
void      joymap_module_shutdown(void);
joymap_t *joymap_load(const char *path);
void      joymap_free(joymap_t *joymap);

#endif
