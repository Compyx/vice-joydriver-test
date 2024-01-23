# VICE joystick driver testing

Small test program to figure out how to query joysticks on various OSes for new
joystick drivers for the non-SDL UIs.
Very much a work in progress.


# Current status (2024-01-21)

## Linux

Scanning devices for capabilities works, polling works.

# BSD

Scanning devices for capabilities works. No polling yet.

# Windows

Scanning devices for capabilities works. No polling yet.

# MacOS

Unsupported.


## Prerequisites:

### Linux

- libevdev (`sudo apt install libevdev-dev`)

### NetBSD

- none

### FreeBSD

- none

### Windows

- msys2

### MacOS

Unsupported.


## Command line program

The test driver program is currently called `vice-joydriver-test`, and supports
a few command line options:

| Option            | Argument     | Description                                      |
| ----------------- | ------------ | ------------------------------------------------ |
| `--help`          |              | Show help                                        |
| `--version`       |              | Show program version                             |
| `--verbose`       |              | Be verbose (limited support)                     |
| `--list-devices`  |              | Show list of joystick devices discovered         |
| `--list-axes`     |              | List axes of device(s)                           |
| `--list-buttons`  |              | List buttons of device(s))                       |
| `--list-hats`     |              | List hats of device(s))                          |
| `--poll`          |              | Poll device for events                           |
| `--poll-interval` | milliseconds | Set interval between polls (default is 100 msec) |


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
