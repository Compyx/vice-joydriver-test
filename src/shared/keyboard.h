/** \file   keyboard.h
 * \brief   Partial copy of VICE's src/keyboard.h
 */

#ifndef VICE_KEYBOARD_H
#define VICE_KEYBOARD_H

/* Maximum of keyboard array (CBM-II values
 * (8 for C64/VIC20, 10 for PET, 11 for C128; we need max).  */
#define KBD_ROWS    16

/* (This is actually the same for all the machines.) */
/* (All have 8, except CBM-II that has 6) */
#define KBD_COLS    8

/* negative rows/columns for extra keys */
#define KBD_ROW_JOY_KEYMAP_A    -1
#define KBD_ROW_JOY_KEYMAP_B    -2

#define KBD_ROW_RESTORE_1       -3
#define KBD_ROW_RESTORE_2       -3
#define KBD_COL_RESTORE_1        0
#define KBD_COL_RESTORE_2        1

#define KBD_ROW_4080COLUMN      -4
#define KBD_ROW_CAPSLOCK        -4
#define KBD_COL_4080COLUMN       0
#define KBD_COL_CAPSLOCK         1

#define KBD_ROW_JOY_KEYPAD      -5

/* joystick port attached keypad */
#define KBD_JOY_KEYPAD_ROWS      4
#define KBD_JOY_KEYPAD_COLS      5

#define KBD_JOY_KEYPAD_NUMKEYS   (KBD_JOY_KEYPAD_ROWS * KBD_JOY_KEYPAD_COLS)

#define KBD_MOD_LSHIFT      (1 << 0)
#define KBD_MOD_RSHIFT      (1 << 1)
#define KBD_MOD_LCTRL       (1 << 2)
#define KBD_MOD_RCTRL       (1 << 3)
#define KBD_MOD_LALT        (1 << 4)
#define KBD_MOD_RALT        (1 << 5)
#define KBD_MOD_SHIFTLOCK   (1 << 6)

#define KBD_CUSTOM_NONE     0
#define KBD_CUSTOM_RESTORE1 1
#define KBD_CUSTOM_RESTORE2 2
#define KBD_CUSTOM_CAPS     3
#define KBD_CUSTOM_4080     4
#define KBD_CUSTOM_NUM      5

#endif
