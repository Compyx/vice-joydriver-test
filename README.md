# VICE joystick driver testing

Small test program to figure out how to query joysticks on various OSes for new
joystick drivers for the non-SDL UIs in [VICE](https://github.com/VICE-Team/svn-mirror/).
Also a playground for implementing joymap files per-device with a human-readable
syntax and the possibility to specify calibration of inputs.

Very much a work in progress.


## Current status (2024-05-14)

### Shared code

Custom mappings (via .vjm files) are partially implemented: axes, buttons and
hats can be mapped to emulated devices' buttons and directions, emulated keys
and UI actions.
Still **TODO** are non-binary POT values (e.g. mapping axes to mouse or paddle
inputs).

Calibration of inputs is still **TODO**, but being worked on: handling of axes'
custom threshold is implemented.

### SDL2

Scanning devices for capabilities work, polling works. Button, axis and hat
events are passed to the generic joystick code. The current implementation uses
the simple `SDL_Joystick` interface, perhaps using the newer `SDL_GameController`
interface gives us more useful info than the (somewhat limited) old interface.

### Linux

Scanning devices for capabilities works, polling works. Button, axis and hat
events are passed to the generic joystick code.

### BSD

Scanning devices for capabilities works, polling works. Button, axis and hat
events are passed to the generic joystick code.

### Windows

Scanning devices for capabilities works, polling works. Button, axis and hat (POV)
events are passed to the generic joystick code.

### MacOS

**Unsupported** due to budgetary constraints.


## Prerequisites:

### Linux

- libevdev (`sudo apt install libevdev-dev`)
- SDL2 (`sudo apt install libsdl2-dev`)

### NetBSD

- none

### FreeBSD

- none

### Windows

- msys2 (See https://www.msys2.org/wiki/MSYS2-installation/)
- SDL2 (`pacman -S ${MINGW_PACKAGE_PREFIX}-SDL2`

### MacOS

Unsupported.


## Command line program

The test driver program is currently called `vice-joydriver-test[-sdl]`, and
supports a few command line options:

| Option                  | Argument     | Description                                      |
| ----------------------- | ------------ | ------------------------------------------------ |
| `-h`, `--help`          |              | Show help                                        |
| `--version`             |              | Show program version                             |
| `-v`, `--verbose`       |              | Be verbose (limited support)                     |
| `-d`, `--debug`         |              | Enable debugging messages                        |
| `--list-devices`        |              | Show list of joystick devices discovered         |
| `--list-axes`           |              | List axes of device(s)                           |
| `--list-buttons`        |              | List buttons of device(s))                       |
| `--list-hats`           |              | List hats of device(s))                          |
| `-p`, `--poll`          |              | Poll device for events                           |
| `-i`, `--poll-interval` | milliseconds | Set interval between polls (default is 100 msec) |
| `-m`, `--joymap`        | filename     | Parse joymap and apply to device being polled    |

The `--joymap` option requires a device node/index to be present among the
command line arguments so the joymap can be loaded for said device.

Polling (`--poll`) can be stopped by pressing Ctrl+C. Extra information can be
printed by passing the `--verbose` and `--debug` flags.

### Examples

Usage is fairly simple: `vice-joydriver-test [options] [<device1> <device2> .. <deviceN>]`
Most options require at least one device node/GUID specified on the command
line.

***To avoid having to type the huge GUIDs on Windows, the device arguments can
also be given as 0-based indexes.***

Using `vice-joydriver-test --list-devices` will list all joystick devices
found, when passing `--verbose` the output will be more verbose.

Detailed lists of axes, buttons and hats can be obtained with the `list-axis`,
`--list-buttons` and `--list-hats` options, for devices listed on the command
line.

For example:
`vice-joydriver-test /dev/input/event20 --list-axis --list-buttons --list-hats`

Polling a device can be done with `--poll`, optionally specifying an interval
with `--poll-interval` (default is 100 milliseconds betwee polls):

For example:
`vice-joydriver-test /dev/input/event20 --poll --poll-interval 10`
will poll 100 times per second. Polling can be stopped with SIGINT (Ctrl+C).


## Devices used during testing

The following table lists the devices used for testing (names are taken from
the output of `lsusb`.

| Name                                               | Vendor | Product | Description   |
| -------------------------------------------------- | ------ | ------- | ------------- |
| Logitech, Inc. F710 Wireless Gamepad [XInput Mode] | `046d` | `c21f`  |               |
| Logitech, Inc. F310 Gamepad [DirectInput Mode]     | `046d` | `c216`  |               |
| Manta MM812                                        | `081f` | `e401`  | "Rii GP100 SNES Retro USB Super Nintendo Controller" |
| Saitek PLC Cyborg Force Rumble Pad                 | `06a3` | `ff0c`  |               |
| Saitek PLC ST50 USB                                | `06a3` | `0168`  | An actual joystick, simple flightstick |
| Sony Corp. Batoh Device / Playstation 3 Controller | `054c` | `0268`  |               |

## Joymap files

The joymap files will be split per device, unlike the current implementation of
these files in VICE. The syntax will also be a bit different, making them much
more human-readable and human-editable. See [this document](vjm-syntax.md) for
the new syntax, again a work in progress and subject to change.
