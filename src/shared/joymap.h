/** \file   joymap.h
 * \brief   VICE joymap file (vjm) parsing - header
 *
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef VICE_JOYMAP_H
#define VICE_JOYMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "joyapi-types.h"

#define VJM_VERSION_MAJOR   2
#define VJM_VERSION_MINOR   0


void      joymap_module_init(void);
void      joymap_module_shutdown(void);
joymap_t *joymap_load(const char *path);
void      joymap_dump(joymap_t *joymap);
void      joymap_free(joymap_t *joymap);

#endif
