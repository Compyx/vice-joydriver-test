/*
 * machine.h - Interface to machine-specific implementations.
 *
 * Written by
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Andreas Boose <viceteam@t-online.de>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#ifndef VICE_MACHINE_H
#define VICE_MACHINE_H

/* A little handier way to identify the machine: */
#define VICE_MACHINE_NONE       0

#define VICE_MACHINE_C64        (1U<<0)
#define VICE_MACHINE_C128       (1U<<1)
#define VICE_MACHINE_VIC20      (1U<<2)
#define VICE_MACHINE_PET        (1U<<3)
#define VICE_MACHINE_CBM5x0     (1U<<4)
#define VICE_MACHINE_CBM6x0     (1U<<5)
#define VICE_MACHINE_PLUS4      (1U<<6)
#define VICE_MACHINE_C64DTV     (1U<<7)
#define VICE_MACHINE_C64SC      (1U<<8)
#define VICE_MACHINE_VSID       (1U<<9)
#define VICE_MACHINE_SCPU64     (1U<<10)

#define VICE_MACHINE_ALL        (VICE_MACHINE_C64|VICE_MACHINE_C64SC|VICE_MACHINE_C64DTV|VICE_MACHINE_SCPU64|VICE_MACHINE_C128|VICE_MACHINE_VIC20|VICE_MACHINE_PLUS4|VICE_MACHINE_PET|VICE_MACHINE_CBM5x0|VICE_MACHINE_CBM6x0|VICE_MACHINE_VSID)

#endif
