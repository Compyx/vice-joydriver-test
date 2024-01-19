# VICE joystick driver testing

Small test program to figure out how to query joysticks on various OSes for new
joystick drivers for the non-SDL UIs.
Very much a work in progress.

## Prerequisites:

### Linux

- libevdev (`sudo apt install libevdev-dev`)

### NetBSD

- none

### FreeBSD

- none

### Windows

- msys2


## Command line program

The test driver program is currently called `vice-joydriver-test`, and supports
a few command line switches:

| Switch            | Argument          | Description                                           |
| ----------------- | ----------------- | ----------------------------------------------------- |
| --help            |                   | Show help                                             |
| --version         |                   | Show program version                                  |
| --verbose         |                   | Be verbose (limited support)                          |
| --list-devices    |                   | Show list of joystick devices discovered              |
| --device-node     | device node/GUID  | Select device                                         |
| --list-axes       |                   | List axes of a device (requires --device-node)        |
| --list-buttons    |                   | List buttons of a devices (requires --device-node)    |
| --list-hats       |                   | List hats of a device (requires --device-node)        |
