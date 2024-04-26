/** \file   uiactions.c
 * \brief   UI actions interface
 *
 * System to handle UI actions in an OS/UI-agnostic way.
 *
 * Used by menu structs, hotkeys and joystick mappings. There will be no Doxygen
 * docblocks for most of the defines, since they're self-explanatory. And
 * obviously I will not bitch about keeping the text within 80 columns here :D
 *
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

/*
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
 */

//#include "vice.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "machine.h"

#include "uiactions.h"

/* Enable debugging */
/* #define DEBUG_ACTIONS */

#define ARRAY_LEN(arr)  (sizeof (arr) / sizeof (arr[0]) )


/** \brief  Mapping of action names to descriptions and machine support
 *
 * The 'machine' members is a bitmask that indicates which machines support
 * the action.
 */
typedef struct ui_action_info_private_s {
    int         id;         /**< action ID */
    const char *name;       /**< action name */
    const char *desc;       /**< action description */
    uint32_t    machine;    /**< bitmask indicating which machines support the
                                 action */
} ui_action_info_private_t;


/** \brief  List of UI actions
 */
static const ui_action_info_private_t action_info_list[] = {
    /* smart attach */
    { ACTION_SMART_ATTACH,      "smart-attach",         "Smart-attach a medium to the emulator", VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    /* disk image attach */
    { ACTION_DRIVE_ATTACH_8_0,  "drive-attach-8:0",     "Attach disk to unit 8, drive 0",   VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_ATTACH_8_1,  "drive-attach-8:1",     "Attach disk to unit 8, drive 1",   VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_ATTACH_9_0,  "drive-attach-9:0",     "Attach disk to unit 9, drive 0",   VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_ATTACH_9_1,  "drive-attach-9:1",     "Attach disk to unit 9, drive 1",   VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_ATTACH_10_0, "drive-attach-10:0",    "Attach disk to unit 10, drive 0",  VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_ATTACH_10_1, "drive-attach-10:1",    "Attach disk to unit 10, drive 1",  VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_ATTACH_11_0, "drive-attach-11:0",    "Attach disk to unit 11, drive 0",  VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_ATTACH_11_1, "drive-attach-11:1",    "Attach disk to unit 11, drive 1",  VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    /* disk image create & attach */
    { ACTION_DRIVE_CREATE,      "drive-create",         "Create and attach empty disk image",   VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    /* disk image detach */
    { ACTION_DRIVE_DETACH_8_0,  "drive-detach-8:0",     "Detach disk from unit 8, drive 0",     VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_DETACH_8_1,  "drive-detach-8:1",     "Detach disk from unit 8, drive 1",     VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_DETACH_9_0,  "drive-detach-9:0",     "Detach disk from unit 9, drive 0",     VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_DETACH_9_1,  "drive-detach-9:1",     "Detach disk from unit 9, drive 1",     VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_DETACH_10_0, "drive-detach-10:0",    "Detach disk from unit 10, drive 0",    VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_DETACH_10_1, "drive-detach-10:1",    "Detach disk from unit 10, drive 1",    VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_DETACH_11_0, "drive-detach-11:0",    "Detach disk from unit 11, drive 0",    VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DRIVE_DETACH_11_1, "drive-detach-11:1",    "Detach disk from unit 11, drive 1",    VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    /* fliplist */
    { ACTION_FLIPLIST_ADD_8_0,          "fliplist-add-8:0",         "Add current disk in unit 8, drive 0 to fliplist",          VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_REMOVE_8_0,       "fliplist-remove-8:0",      "Remove current disk in unit 8, drive 0 from fliplist",     VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_NEXT_8_0,         "fliplist-next-8:0",        "Attach next disk in fliplist to unit 8, drive 0",          VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_PREVIOUS_8_0,     "fliplist-previous-8:0",    "Attach previous disk in fliplist to unit 8, drive 0",      VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_LOAD_8_0,         "fliplist-load-8:0",        "Load fliplist for unit 8, drive 0",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_SAVE_8_0,         "fliplist-save-8:0",        "Save fliplist of unit 8, drive 0",                         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_CLEAR_8_0,        "fliplist-clear-8:0",       "Clear fliplist of unit 8, drive 0",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    { ACTION_FLIPLIST_ADD_8_1,          "fliplist-add-8:1",         "Add current disk in unit 8, drive 1 to fliplist",          VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_REMOVE_8_1,       "fliplist-remove-8:1",      "Remove current disk in unit 8, drive 1 from fliplist",     VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_NEXT_8_1,         "fliplist-next-8:1",        "Attach next disk in fliplist to unit 8, drive 1",          VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_PREVIOUS_8_1,     "fliplist-previous-8:1",    "Attach previous disk in fliplist to unit 8, drive 1",      VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_LOAD_8_1,         "fliplist-load-8:1",        "Load fliplist for unit 8, drive 1",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_SAVE_8_1,         "fliplist-save-8:1",        "Save fliplist of unit 8, drive 1",                         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_CLEAR_8_1,        "fliplist-clear-8:1",       "Clear fliplist of unit 8, drive 1",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    { ACTION_FLIPLIST_ADD_9_0,          "fliplist-add-9:0",         "Add current disk in unit 9, drive 0 to fliplist",          VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_REMOVE_9_0,       "fliplist-remove-9:0",      "Remove current disk in unit 9, drive 0 from fliplist",     VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_NEXT_9_0,         "fliplist-next-9:0",        "Attach next disk in fliplist to unit 9, drive 0",          VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_PREVIOUS_9_0,     "fliplist-previous-9:0",    "Attach previous disk in fliplist to unit 9, drive 0",      VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_LOAD_9_0,         "fliplist-load-9:0",        "Load fliplist for unit 9, drive 0",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_SAVE_9_0,         "fliplist-save-9:0",        "Save fliplist of unit 9, drive 0",                         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_CLEAR_9_0,        "fliplist-clear-9:0",       "Clear fliplist of unit 9, drive 0",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    { ACTION_FLIPLIST_ADD_9_1,          "fliplist-add-9:1",         "Add current disk in unit 9, drive 1 to fliplist",          VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_REMOVE_9_1,       "fliplist-remove-9:1",      "Remove current disk in unit 9, drive 1 from fliplist",     VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_NEXT_9_1,         "fliplist-next-9:1",        "Attach next disk in fliplist to unit 9, drive 1",          VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_PREVIOUS_9_1,     "fliplist-previous-9:1",    "Attach previous disk in fliplist to unit 9, drive 1",      VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_LOAD_9_1,         "fliplist-load-9:1",        "Load fliplist for unit 9, drive 1",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_SAVE_9_1,         "fliplist-save-9:1",        "Save fliplist of unit 9, drive 1",                         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_CLEAR_9_1,        "fliplist-clear-9:1",       "Clear fliplist of unit 9, drive 1",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    { ACTION_FLIPLIST_ADD_10_0,         "fliplist-add-10:0",        "Add current disk in unit 10, drive 0 to fliplist",         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_REMOVE_10_0,      "fliplist-remove-10:0",     "Remove current disk in unit 10, drive 0 from fliplist",    VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_NEXT_10_0,        "fliplist-next-10:0",       "Attach next disk in fliplist to unit 10, drive 0",         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_PREVIOUS_10_0,    "fliplist-previous-10:0",   "Attach previous disk in fliplist to unit 10, drive 0",     VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_LOAD_10_0,        "fliplist-load-10:0",       "Load fliplist for unit 10, drive 0",                       VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_SAVE_10_0,        "fliplist-save-10:0",       "Save fliplist of unit 10, drive 0",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_CLEAR_10_0,       "fliplist-clear-10:0",      "Clear fliplist of unit 10, drive 0",                       VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    { ACTION_FLIPLIST_ADD_10_1,         "fliplist-add-10:1",        "Add current disk in unit 10, drive 1 to fliplist",         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_REMOVE_10_1,      "fliplist-remove-10:1",     "Remove current disk in unit 10, drive 1 from fliplist",    VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_NEXT_10_1,        "fliplist-next-10:1",       "Attach next disk in fliplist to unit 10, drive 1",         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_PREVIOUS_10_1,    "fliplist-previous-10:1",   "Attach previous disk in fliplist to unit 10, drive 1",     VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_LOAD_10_1,        "fliplist-load-10:1",       "Load fliplist for unit 10, drive 1",                       VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_SAVE_10_1,        "fliplist-save-10:1",       "Save fliplist of unit 10, drive 1",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_CLEAR_10_1,       "fliplist-clear-10:1",      "Clear fliplist of unit 10, drive 1",                       VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    { ACTION_FLIPLIST_ADD_11_0,         "fliplist-add-11:0",        "Add current disk in unit 11, drive 0 to fliplist",         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_REMOVE_11_0,      "fliplist-remove-11:0",     "Remove current disk in unit 11, drive 0 from fliplist",    VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_NEXT_11_0,        "fliplist-next-11:0",       "Attach next disk in fliplist to unit 11, drive 0",         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_PREVIOUS_11_0,    "fliplist-previous-11:0",   "Attach previous disk in fliplist to unit 11, drive 0",     VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_LOAD_11_0,        "fliplist-load-11:0",       "Load fliplist for unit 11, drive 0",                       VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_SAVE_11_0,        "fliplist-save-11:0",       "Save fliplist of unit 11, drive 0",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_CLEAR_11_0,       "fliplist-clear-11:0",      "Clear fliplist of unit 11, drive 0",                       VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    { ACTION_FLIPLIST_ADD_11_1,         "fliplist-add-11:1",        "Add current disk in unit 11, drive 1 to fliplist",         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_REMOVE_11_1,      "fliplist-remove-11:1",     "Remove current disk in unit 11, drive 1 from fliplist",    VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_NEXT_11_1,        "fliplist-next-11:1",       "Attach next disk in fliplist to unit 11, drive 1",         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_PREVIOUS_11_1,    "fliplist-previous-11:1",   "Attach previous disk in fliplist to unit 11, drive 1",     VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_LOAD_11_1,        "fliplist-load-11:1",       "Load fliplist for unit 11, drive 1",                       VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_SAVE_11_1,        "fliplist-save-11:1",       "Save fliplist of unit 11, drive 1",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FLIPLIST_CLEAR_11_1,       "fliplist-clear-11:1",      "Clear fliplist of unit 11, drive 1",                       VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    /* datasette image */
    { ACTION_TAPE_ATTACH_1,     "tape-attach-1",        "Attach tape to datasette 1",               VICE_MACHINE_ALL^VICE_MACHINE_C64DTV^VICE_MACHINE_SCPU64^VICE_MACHINE_VSID },
    { ACTION_TAPE_ATTACH_2,     "tape-attach-2",        "Attach tape to datasette 2",               VICE_MACHINE_PET },
    { ACTION_TAPE_DETACH_1,     "tape-detach-1",        "Detach tape from datasette 1",             VICE_MACHINE_ALL^VICE_MACHINE_C64DTV^VICE_MACHINE_SCPU64^VICE_MACHINE_VSID },
    { ACTION_TAPE_DETACH_2,     "tape-detach-2",        "Detach tape from datasette 2",             VICE_MACHINE_PET },
    { ACTION_TAPE_CREATE_1,     "tape-create-1",        "Create tape and attach to datasette 1",    VICE_MACHINE_ALL^VICE_MACHINE_C64DTV^VICE_MACHINE_SCPU64^VICE_MACHINE_VSID },
    { ACTION_TAPE_CREATE_2,     "tape-create-2",        "Create tape and attach to datasette 2",    VICE_MACHINE_PET },

    /* datasette controls */
    { ACTION_TAPE_RECORD_1,         "tape-record-1",        "Press RECORD on datasette 1",  VICE_MACHINE_ALL^VICE_MACHINE_C64DTV^VICE_MACHINE_SCPU64^VICE_MACHINE_VSID },
    { ACTION_TAPE_RECORD_2,         "tape-record-2",        "Press RECORD on datasette 2",  VICE_MACHINE_PET },
    { ACTION_TAPE_PLAY_1,           "tape-play-1",          "Press PLAY on datasette 1",    VICE_MACHINE_ALL^VICE_MACHINE_C64DTV^VICE_MACHINE_SCPU64^VICE_MACHINE_VSID },
    { ACTION_TAPE_PLAY_2,           "tape-play-2",          "Press PLAY on datasette 2",    VICE_MACHINE_PET },
    { ACTION_TAPE_REWIND_1,         "tape-rewind-1",        "Press REWIND on datasette 1",  VICE_MACHINE_ALL^VICE_MACHINE_C64DTV^VICE_MACHINE_SCPU64^VICE_MACHINE_VSID },
    { ACTION_TAPE_REWIND_2,         "tape-rewind-2",        "Press REWIND on datasette 2",  VICE_MACHINE_PET },
    { ACTION_TAPE_FFWD_1,           "tape-ffwd-1",          "Press FFWD on datasette 1",    VICE_MACHINE_ALL^VICE_MACHINE_C64DTV^VICE_MACHINE_SCPU64^VICE_MACHINE_VSID },
    { ACTION_TAPE_FFWD_2,           "tape-ffwd-2",          "Press FFWD on datasette 2",    VICE_MACHINE_PET },
    { ACTION_TAPE_STOP_1,           "tape-stop-1",          "Press STOP on datasette 1",    VICE_MACHINE_ALL^VICE_MACHINE_C64DTV^VICE_MACHINE_SCPU64^VICE_MACHINE_VSID },
    { ACTION_TAPE_STOP_2,           "tape-stop-2",          "Press STOP on datasette 2",    VICE_MACHINE_PET },
    { ACTION_TAPE_RESET_1,          "tape-reset-1",         "Reset datasette 1",            VICE_MACHINE_ALL^VICE_MACHINE_C64DTV^VICE_MACHINE_SCPU64^VICE_MACHINE_VSID },
    { ACTION_TAPE_RESET_2,          "tape-reset-2",         "Reset datasette 2",            VICE_MACHINE_PET },
    { ACTION_TAPE_RESET_COUNTER_1,  "tape-reset-counter-1", "Reset datasette 1 counter",    VICE_MACHINE_ALL^VICE_MACHINE_C64DTV^VICE_MACHINE_SCPU64^VICE_MACHINE_VSID },
    { ACTION_TAPE_RESET_COUNTER_2,  "tape-reset-counter-2", "Reset datasette 2 counter",    VICE_MACHINE_PET },

    /* cartridge items */
    { ACTION_CART_ATTACH,               "cart-attach",              "Attach CRT cartridge image",           (VICE_MACHINE_C64|VICE_MACHINE_C64SC|VICE_MACHINE_SCPU64|VICE_MACHINE_C128|
                                                                                                             VICE_MACHINE_VIC20|VICE_MACHINE_PLUS4|VICE_MACHINE_CBM6x0) },
    { ACTION_CART_ATTACH_RAW,           "cart-attach-raw",          "Attach raw cartridge image",           (VICE_MACHINE_C64|VICE_MACHINE_C64SC|VICE_MACHINE_SCPU64|VICE_MACHINE_C128|
                                                                                                             VICE_MACHINE_VIC20|VICE_MACHINE_PLUS4|VICE_MACHINE_CBM6x0) },
    { ACTION_CART_ATTACH_RAW_1000,      "cart-attach-raw-1000",     "Attach raw cartridge image at $1000",  VICE_MACHINE_CBM6x0 },
    { ACTION_CART_ATTACH_RAW_2000,      "cart-attach-raw-2000",     "Attach raw cartridge image at $2000",  VICE_MACHINE_CBM6x0|VICE_MACHINE_VIC20 },
    { ACTION_CART_ATTACH_RAW_4000,      "cart-attach-raw-4000",     "Attach raw cartridge image at $4000",  VICE_MACHINE_CBM6x0|VICE_MACHINE_VIC20 },
    { ACTION_CART_ATTACH_RAW_6000,      "cart-attach-raw-6000",     "Attach raw cartridge image at $6000",  VICE_MACHINE_CBM6x0|VICE_MACHINE_VIC20 },
    { ACTION_CART_ATTACH_RAW_A000,      "cart-attach-raw-a000",     "Attach raw cartridge image at $A000",  VICE_MACHINE_VIC20 },
    { ACTION_CART_ATTACH_RAW_B000,      "cart-attach-raw-b000",     "Attach raw cartridge image at $B000",  VICE_MACHINE_VIC20 },
    { ACTION_CART_ATTACH_RAW_BEHRBONZ,  "cart-attach-raw-behrbonz", "Attach Behr Bonz cartridge image",     VICE_MACHINE_VIC20 },
    { ACTION_CART_ATTACH_RAW_FINAL,     "cart-attach-raw-final",    "Attach Final Expansion cartridge image", VICE_MACHINE_VIC20 },
    { ACTION_CART_ATTACH_RAW_MEGACART,  "cart-attach-raw-megacart", "Attach Mega-Cart image",               VICE_MACHINE_VIC20 },
    { ACTION_CART_ATTACH_RAW_ULTIMEM,   "cart-attach-raw-ultimem",  "Attach UltiMem cartridge image",       VICE_MACHINE_VIC20 },
    { ACTION_CART_ATTACH_RAW_VICFP,     "cart-attach-raw-vicfp",    "Attach Vic Flash Plugin cartridge image", VICE_MACHINE_VIC20 },
    { ACTION_CART_ATTACH_RAW_JACINT1MB, "cart-attach-raw-jacint1mb", "Attach 1MB Cartridge image",           VICE_MACHINE_PLUS4 },
    { ACTION_CART_ATTACH_RAW_MAGIC,     "cart-attach-raw-magic",    "Attach c264 magic cart image",         VICE_MACHINE_PLUS4 },
    { ACTION_CART_ATTACH_RAW_MULTI,     "cart-attach-raw-multi",    "Attach multi cart image",              VICE_MACHINE_PLUS4 },
    { ACTION_CART_ATTACH_RAW_C1_FULL,   "cart-attach-raw-c1-full",  "Attach full C1 cartridge image",       VICE_MACHINE_PLUS4 },
    { ACTION_CART_ATTACH_RAW_C1_LOW,    "cart-attach-raw-c1-low",   "Attach low C1 cartridge image",        VICE_MACHINE_PLUS4 },
    { ACTION_CART_ATTACH_RAW_C1_HIGH,   "cart-attach-raw-c1-high",  "Attach high C1 cartridge image",       VICE_MACHINE_PLUS4 },
    { ACTION_CART_ATTACH_RAW_C2_FULL,   "cart-attach-raw-c2-full",  "Attach full C2 cartridge image",       VICE_MACHINE_PLUS4 },
    { ACTION_CART_ATTACH_RAW_C2_LOW,    "cart-attach-raw-c2-low",   "Attach low C2 cartridge image",        VICE_MACHINE_PLUS4 },
    { ACTION_CART_ATTACH_RAW_C2_HIGH,   "cart-attach-raw-c2-high",  "Attach high C2 cartridge image",       VICE_MACHINE_PLUS4 },
    { ACTION_CART_DETACH,               "cart-detach",              "Detach cartridge",                     (VICE_MACHINE_C64|VICE_MACHINE_C64SC|VICE_MACHINE_SCPU64|
                                                                                                             VICE_MACHINE_C128|VICE_MACHINE_PLUS4|VICE_MACHINE_CBM6x0) },
    { ACTION_CART_FREEZE,               "cart-freeze",              "Press cartridge freeze button",        (VICE_MACHINE_C64|VICE_MACHINE_C64SC|VICE_MACHINE_SCPU64|VICE_MACHINE_C128) },
    { ACTION_CART_DETACH_1000,          "cart-detach-1000",         "Detach cartridge image at $1000",      VICE_MACHINE_CBM6x0 },
    { ACTION_CART_DETACH_2000,          "cart-detach-2000",         "Detach cartridge image at $2000",      VICE_MACHINE_CBM6x0 },
    { ACTION_CART_DETACH_4000,          "cart-detach-4000",         "Detach cartridge image at $4000",      VICE_MACHINE_CBM6x0 },
    { ACTION_CART_DETACH_6000,          "cart-detach-6000",         "Detach cartridge image at $6000",      VICE_MACHINE_CBM6x0 },

    /* open monitor */
    { ACTION_MONITOR_OPEN,      "monitor-open",         "Open monitor",                         VICE_MACHINE_ALL },

    /* reset items */
    { ACTION_MACHINE_RESET_CPU,      "machine-reset-cpu",       "Reset the machine CPU",                VICE_MACHINE_ALL },
    { ACTION_MACHINE_POWER_CYCLE,    "machine-power-cycle",     "Power cycle the machine",              VICE_MACHINE_ALL },

    { ACTION_RESET_DRIVE_8,          "reset-drive-8",           "Reset drive 8",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_RESET_DRIVE_8_CONFIG,   "reset-drive-8-config",    "Reset drive 8 in configuration mode",  VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_RESET_DRIVE_8_INSTALL,  "reset-drive-8-install",   "Reset drive 8 in installation mode",   VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_RESET_DRIVE_9,          "reset-drive-9",           "Reset drive 9",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_RESET_DRIVE_9_CONFIG,   "reset-drive-9-config",    "Reset drive 9 in configuration mode",  VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_RESET_DRIVE_9_INSTALL,  "reset-drive-9-install",   "Reset drive 9 in installation mode",   VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_RESET_DRIVE_10,         "reset-drive-10",          "Reset drive 10",                       VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_RESET_DRIVE_10_CONFIG,  "reset-drive-10-config",   "Reset drive 10 in configuration mode", VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_RESET_DRIVE_10_INSTALL, "reset-drive-10-install",  "Reset drive 10 in installation mode",  VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_RESET_DRIVE_11,         "reset-drive-11",          "Reset drive 11",                       VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_RESET_DRIVE_11_CONFIG,  "reset-drive-11-config",   "Reset drive 11 in configuration mode", VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_RESET_DRIVE_11_INSTALL, "reset-drive-11-install",  "Reset drive 11 in installation mode",  VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    /* quit emulator */
    { ACTION_QUIT,              "quit",                 "Quit emulator",                        VICE_MACHINE_ALL },

    /* edit items */
    { ACTION_EDIT_COPY,         "edit-copy",            "Copy screen content to clipboard",     VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_EDIT_PASTE,        "edit-paste",           "Paste clipboard content into machine", VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    /* pause, warp, advance-frame */
    { ACTION_PAUSE_TOGGLE,      "pause-toggle",         "Toggle Pause",                         VICE_MACHINE_ALL },
    { ACTION_ADVANCE_FRAME,     "advance-frame",        "Advance emulation one frame",          VICE_MACHINE_ALL },
    { ACTION_WARP_MODE_TOGGLE,  "warp-mode-toggle",     "Toggle Warp Mode",                     VICE_MACHINE_ALL },

    /* CPU speed presets and custom speed */
    { ACTION_SPEED_CPU_10,      "speed-cpu-10",         "Set CPU speed to 10%",                 VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SPEED_CPU_25,      "speed-cpu-25",         "Set CPU speed to 25%",                 VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SPEED_CPU_50,      "speed-cpu-50",         "Set CPU speed to 50%",                 VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SPEED_CPU_100,     "speed-cpu-100",        "Set CPU speed to 100%",                VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SPEED_CPU_200,     "speed-cpu-200",        "Set CPU speed to 200%",                VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SPEED_CPU_CUSTOM,  "speed-cpu-custom",     "Set custom CPU speed",                 VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    /* video clock */
    { ACTION_SPEED_FPS_50,      "speed-fps-50",         "Set video clock to 50Hz",              VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SPEED_FPS_60,      "speed-fps-60",         "Set video clock to 60Hz",              VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SPEED_FPS_CUSTOM,  "speed-fps-custom",     "Set custom video clock",               VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SPEED_FPS_REAL,    "speed-fps-real",       "Set real video clock",                 VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    /* fullscreen, fullscreen decs, restore display */
    { ACTION_FULLSCREEN_TOGGLE,                 "fullscreen-toggle",                "Toggle fullscreen",                        VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_FULLSCREEN_DECORATIONS_TOGGLE,     "fullscreen-decorations-toggle",    "Show menu/status in fullscreen",           VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SHOW_STATUSBAR_TOGGLE,             "show-statusbar-toggle",            "Show status bar",                          VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SHOW_STATUSBAR_SECONDARY_TOGGLE,   "show-statusbar-secondary-toggle",  "Show secondary status bar",                VICE_MACHINE_C128 },
    { ACTION_RESTORE_DISPLAY,                   "restore-display",                  "Resize window to fit content",             VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    /* joystick, mouse etc */
    { ACTION_SWAP_CONTROLPORT_TOGGLE,   "swap-controlport-toggle",  "Swap controlport joysticks",   (VICE_MACHINE_C64|VICE_MACHINE_C64SC|VICE_MACHINE_C64DTV|VICE_MACHINE_SCPU64|
                                                                                                     VICE_MACHINE_C128|VICE_MACHINE_PLUS4|VICE_MACHINE_CBM5x0) },
    { ACTION_MOUSE_GRAB_TOGGLE,         "mouse-grab-toggle",        "Toggle Mouse Grab",            VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_KEYSET_JOYSTICK_TOGGLE,    "keyset-joystick-toggle",   "Allow keyset joysticks",       VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    /* settings items */
    { ACTION_SETTINGS_DIALOG,       "settings-dialog",      "Open settings",                        VICE_MACHINE_ALL },
    { ACTION_SETTINGS_LOAD,         "settings-load",        "Load settings",                        VICE_MACHINE_ALL },
    { ACTION_SETTINGS_LOAD_FROM,    "settings-load-from",   "Load settings from custom file",       VICE_MACHINE_ALL },
    { ACTION_SETTINGS_LOAD_EXTRA,   "settings-load-extra",  "Load additional settings",             VICE_MACHINE_ALL },
    { ACTION_SETTINGS_SAVE,         "settings-save",        "Save settings",                        VICE_MACHINE_ALL },
    { ACTION_SETTINGS_SAVE_TO,      "settings-save-to",     "Save settings to custom file",         VICE_MACHINE_ALL },
    { ACTION_SETTINGS_DEFAULT,      "settings-default",     "Restore default settings",             VICE_MACHINE_ALL },

    /* snapshots, media recording, events */
    { ACTION_SNAPSHOT_LOAD,             "snapshot-load",            "Load snapshot file",               VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SNAPSHOT_SAVE,             "snapshot-save",            "Save snapshot file",               VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SNAPSHOT_QUICKLOAD,        "snapshot-quickload",       "Quickload snapshot",               VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SNAPSHOT_QUICKSAVE,        "snapshot-quicksave",       "Quicksave snapshot",               VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_HISTORY_RECORD_START,      "history-record-start",     "Start recording events",           VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_HISTORY_RECORD_STOP,       "history-record-stop",      "Stop recording events",            VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_HISTORY_PLAYBACK_START,    "history-playback-start",   "Start playing back events",        VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_HISTORY_PLAYBACK_STOP,     "history-playback-stop",    "Stop playing back events",         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_HISTORY_MILESTONE_SET,     "history-milestone-set",    "Set recording milestone",          VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_HISTORY_MILESTONE_RESET,   "history-milestone-reset",  "Return to recording milestone",    VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_MEDIA_RECORD,              "media-record",             "Start recording media",            VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_MEDIA_RECORD_AUDIO,        "media-record-audio",       "Start recording audio",            VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_MEDIA_RECORD_SCREENSHOT,   "media-record-screenshot",  "Take screenshot",                  VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_MEDIA_RECORD_VIDEO,        "media-record-video",       "Start recording video",            VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_MEDIA_STOP,                "media-stop",               "Stop media recording",             VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_SCREENSHOT_QUICKSAVE,      "screenshot-quicksave",     "Quiksave screenshot",              VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    /* debug items */
#ifdef DEBUG
    { ACTION_DEBUG_TRACE_MODE,              "debug-trace-mode",             "Select machine/drive CPU trace mode",  VICE_MACHINE_ALL },
    { ACTION_DEBUG_TRACE_CPU_TOGGLE,        "debug-trace-cpu-toggle",       "Toggle CPU trace",                     VICE_MACHINE_ALL },
    { ACTION_DEBUG_TRACE_IEC_TOGGLE,        "debug-trace-iec-toggle",       "Toggle IEC bus trace",                 VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DEBUG_TRACE_IEEE488_TOGGLE,    "debug-trace-ieee488-toggle",   "Toggle IEEE-488 bus trace",            VICE_MACHINE_ALL^VICE_MACHINE_C64DTV^VICE_MACHINE_VSID },
    { ACTION_DEBUG_TRACE_DRIVE_8_TOGGLE,    "debug-trace-drive-8-toggle",   "Toggle drive 8 CPU trace",             VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DEBUG_TRACE_DRIVE_9_TOGGLE,    "debug-trace-drive-9-toggle",   "Toggle drive 9 CPU trace",             VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DEBUG_TRACE_DRIVE_10_TOGGLE,   "debug-trace-drive-10-toggle",  "Toggle drive 10 CPU trace",            VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DEBUG_TRACE_DRIVE_11_TOGGLE,   "debug-trace-drive-11-toggle",  "Toggle drive 11 CPU trace",            VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_DEBUG_AUTOPLAYBACK_FRAMES,     "debug-autoplayback-frames",    "Set autoplayback frames",              VICE_MACHINE_ALL },
    { ACTION_DEBUG_CORE_DUMP_TOGGLE,        "debug-core-dump-toggle",       "Toggle saving core dump",              VICE_MACHINE_ALL },
    /* DTV-specific */
    { ACTION_DEBUG_BLITTER_LOG_TOGGLE,      "debug-blitter-log-toggle",     "Toggle blitter logging",               VICE_MACHINE_C64DTV },
    { ACTION_DEBUG_DMA_LOG_TOGGLE,          "debug-dma-log-toggle",         "Toggle DMA logging",                   VICE_MACHINE_C64DTV },
    { ACTION_DEBUG_FLASH_LOG_TOGGLE,        "debug-flash-log-toggle",       "Toggle Flash logging",                 VICE_MACHINE_C64DTV },
#endif

    /* Help items */
    { ACTION_HELP_MANUAL,       "help-manual",          "Browse VICE manual",           VICE_MACHINE_ALL },
    { ACTION_HELP_COMMAND_LINE, "help-command-line",    "Show command line options",    VICE_MACHINE_ALL },
    { ACTION_HELP_COMPILE_TIME, "help-compile-time",    "Show compile time features",   VICE_MACHINE_ALL },
    /* XXX: Is this still valid? We have a hotkeys editor now. */
    { ACTION_HELP_HOTKEYS,      "help-hotkeys",         "Show hotkeys",                 VICE_MACHINE_ALL },
    { ACTION_HELP_ABOUT,        "help-about",           "Show About dialog",            VICE_MACHINE_ALL },

    /* Hotkeys items */
    { ACTION_HOTKEYS_CLEAR,     "hotkeys-clear",        "Clear all hotkeys",                VICE_MACHINE_ALL },
    { ACTION_HOTKEYS_DEFAULT,   "hotkeys-default",      "Load default hotkeys",             VICE_MACHINE_ALL },
    { ACTION_HOTKEYS_LOAD,      "hotkeys-load",         "Load hotkeys from current file",   VICE_MACHINE_ALL },
    { ACTION_HOTKEYS_LOAD_FROM, "hotkeys-load-from",    "Load hotkeys from custom file",    VICE_MACHINE_ALL },
    { ACTION_HOTKEYS_SAVE,      "hotkeys-save",         "Save hotkeys to current file",     VICE_MACHINE_ALL },
    { ACTION_HOTKEYS_SAVE_TO,   "hotkeys-save-to",      "Save hotkeys to custom file",      VICE_MACHINE_ALL },

    /* VSID-specific items */
    { ACTION_PSID_LOAD,                 "psid-load",                "Load PSID file",           VICE_MACHINE_VSID },
    { ACTION_PSID_OVERRIDE_TOGGLE,      "psid-override-toggle",     "Override PSID settings",   VICE_MACHINE_VSID },

    { ACTION_PSID_SUBTUNE_1,            "psid-subtune-1",           "Play subtune #1",          VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_2,            "psid-subtune-2",           "Play subtune #2",          VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_3,            "psid-subtune-3",           "Play subtune #3",          VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_4,            "psid-subtune-4",           "Play subtune #4",          VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_5,            "psid-subtune-5",           "Play subtune #5",          VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_6,            "psid-subtune-6",           "Play subtune #6",          VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_7,            "psid-subtune-7",           "Play subtune #7",          VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_8,            "psid-subtune-8",           "Play subtune #8",          VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_9,            "psid-subtune-9",           "Play subtune #9",          VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_10,           "psid-subtune-10",          "Play subtune #10",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_11,           "psid-subtune-11",          "Play subtune #11",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_12,           "psid-subtune-12",          "Play subtune #12",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_13,           "psid-subtune-13",          "Play subtune #13",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_14,           "psid-subtune-14",          "Play subtune #14",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_15,           "psid-subtune-15",          "Play subtune #15",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_16,           "psid-subtune-16",          "Play subtune #16",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_17,           "psid-subtune-17",          "Play subtune #17",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_18,           "psid-subtune-18",          "Play subtune #18",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_19,           "psid-subtune-19",          "Play subtune #19",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_20,           "psid-subtune-20",          "Play subtune #20",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_21,           "psid-subtune-21",          "Play subtune #21",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_22,           "psid-subtune-22",          "Play subtune #22",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_23,           "psid-subtune-23",          "Play subtune #23",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_24,           "psid-subtune-24",          "Play subtune #24",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_25,           "psid-subtune-25",          "Play subtune #25",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_26,           "psid-subtune-26",          "Play subtune #26",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_27,           "psid-subtune-27",          "Play subtune #27",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_28,           "psid-subtune-28",          "Play subtune #28",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_29,           "psid-subtune-29",          "Play subtune #29",         VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_30,           "psid-subtune-30",          "Play subtune #30",         VICE_MACHINE_VSID },

    { ACTION_PSID_SUBTUNE_DEFAULT,      "psid-subtune-default",     "Play default subtune",     VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_NEXT,         "psid-subtune-next",        "Play next subtune",        VICE_MACHINE_VSID },
    { ACTION_PSID_SUBTUNE_PREVIOUS,     "psid-subtune-previous",    "Play previous subtune",    VICE_MACHINE_VSID },

    { ACTION_PSID_PLAY,                 "psid-play",                "Play",                     VICE_MACHINE_VSID },
    { ACTION_PSID_PAUSE,                "psid-pause",               "Pause playback",           VICE_MACHINE_VSID },
    { ACTION_PSID_STOP,                 "psid-stop",                "Stop playback",            VICE_MACHINE_VSID },
    { ACTION_PSID_FFWD,                 "psid-ffwd",                "Fast forward",             VICE_MACHINE_VSID },
    { ACTION_PSID_LOOP_TOGGLE,          "psid-loop-toggle",         "Toggle looping",           VICE_MACHINE_VSID },

    { ACTION_PSID_PLAYLIST_FIRST,       "psid-playlist-first",      "Play first tune in the playlist",      VICE_MACHINE_VSID },
    { ACTION_PSID_PLAYLIST_PREVIOUS,    "psid-playlist-previous",   "Play previous tune in the playlist",   VICE_MACHINE_VSID },
    { ACTION_PSID_PLAYLIST_NEXT,        "psid-playlist-next",       "Play next tune in the playlist",       VICE_MACHINE_VSID },
    { ACTION_PSID_PLAYLIST_LAST,        "psid-playlist-last",       "Play last tune in the playlist",       VICE_MACHINE_VSID },
    { ACTION_PSID_PLAYLIST_ADD,         "psid-playlist-add",        "Add files to the playlist",            VICE_MACHINE_VSID },
    { ACTION_PSID_PLAYLIST_LOAD,        "psid-playlist-load",       "Load a playlist",                      VICE_MACHINE_VSID },
    { ACTION_PSID_PLAYLIST_SAVE,        "psid-playlist-save",       "Save the playlist",                    VICE_MACHINE_VSID },
    { ACTION_PSID_PLAYLIST_CLEAR,       "psid-playlist-clear",      "Clear the playlist",                   VICE_MACHINE_VSID },

    /* xpet */
    { ACTION_DIAGNOSTIC_PIN_TOGGLE,     "diagnostic-pin-toggle",    "Toggle PET userport diagnostic pin",   VICE_MACHINE_PET },

    /* printers */
    { ACTION_PRINTER_FORMFEED_4,        "printer-formfeed-4",       "Send form feed to printer #4",         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_PRINTER_FORMFEED_5,        "printer-formfeed-5",       "Send form feed to printer #5",         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_PRINTER_FORMFEED_6,        "printer-formfeed-6",       "Send form feed to plotter #6",         VICE_MACHINE_ALL^VICE_MACHINE_VSID },
    { ACTION_PRINTER_FORMFEED_USERPORT, "printer-formfeed-userport", "Send form feed to userport printer",  VICE_MACHINE_C64|VICE_MACHINE_C64SC|VICE_MACHINE_SCPU64|VICE_MACHINE_C128|VICE_MACHINE_VIC20|VICE_MACHINE_PET|VICE_MACHINE_CBM6x0 },

    { ACTION_VIRTUAL_KEYBOARD,          "virtual-keyboard",         "Activate virtual keyboard",            VICE_MACHINE_ALL^VICE_MACHINE_VSID },

    /* Border modes (also invalid for x128's VDC window) */
    { ACTION_BORDER_MODE_NORMAL,        "border-mode-normal",       "Set border mode to Normal",            VICE_MACHINE_ALL^VICE_MACHINE_VSID^VICE_MACHINE_CBM6x0^VICE_MACHINE_PET },
    { ACTION_BORDER_MODE_FULL,          "border-mode-full",         "Set border mode to Full",              VICE_MACHINE_ALL^VICE_MACHINE_VSID^VICE_MACHINE_CBM6x0^VICE_MACHINE_PET },
    { ACTION_BORDER_MODE_DEBUG,         "border-mode-debug",        "Set border mode to Debug",             VICE_MACHINE_ALL^VICE_MACHINE_VSID^VICE_MACHINE_CBM6x0^VICE_MACHINE_PET },
    { ACTION_BORDER_MODE_NONE,          "border-mode-none",         "Set border mode to None",              VICE_MACHINE_ALL^VICE_MACHINE_VSID^VICE_MACHINE_CBM6x0^VICE_MACHINE_PET },

    /* SCPU64 switches */
    { ACTION_SCPU_JIFFY_SWITCH_TOGGLE,  "scpu-jiffy-switch-toggle", "Toggle SCPU JiffyDOS switch",          VICE_MACHINE_SCPU64 },
    { ACTION_SCPU_SPEED_SWITCH_TOGGLE,  "scpu-speed-switch-toggle", "Toggle SCPU Speed switch",             VICE_MACHINE_SCPU64 },

    { ACTION_INVALID, NULL, NULL, 0 }
};



/** \brief  Get "private" info about a UI action
 *
 * \param[in]   id  action ID
 *
 * \return  pointer to info or `NULL` on failure
 */
static const ui_action_info_private_t *get_info_private(int action)
{
    if (action > ACTION_NONE) {
        int i = 0;

        while (action_info_list[i].id > ACTION_NONE) {
            if (action_info_list[i].id == action) {
                return &action_info_list[i];
            }
            i++;
        }
    }
    return NULL;
}


/** \brief  Get action ID by name
 *
 * Get the action name as used by the vhk files.
 *
 * \param[in]   name    action name
 *
 * \return  ID or -1 when not found
 */
int ui_action_get_id(const char *name)
{
    if (name != NULL && *name != '\0') {
        int i = 0;

        while (action_info_list[i].id > ACTION_NONE) {
            if (strcmp(action_info_list[i].name, name) == 0) {
                return action_info_list[i].id;
            }
            i++;
        }
    }
    return ACTION_INVALID;
}


/** \brief  Get action name by ID
 *
 * \param[in]   action  UI action ID
 *
 * \return  action name or NULL when not found
 *
 * \note    Also returns NULL for NONE and INVALID
 */
const char *ui_action_get_name(int action)
{
    const ui_action_info_private_t *info = get_info_private(action);

    return info != NULL ? info->name : NULL;
}


/** \brief  Get description of an action
 *
 * Looks up the description for \a action.
 *
 * \param[in]   action   UI action ID
 *
 * \return  description or `NULL` when \a action not found
 */
const char *ui_action_get_desc(int action)
{
    const ui_action_info_private_t *info = get_info_private(action);

    return info != NULL ? info->desc : NULL;
}
