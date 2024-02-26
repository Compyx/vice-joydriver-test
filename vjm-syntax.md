# VICE joymap file syntax

## General syntax

Integer values can be specified in decimal or hexadecimal notation. Strings with
spaces in them can be specified using double quotation marks (**`"`**).
Whitespace is used to separate keyword arguments.

## Keywords

A .vjm file consists of a number of keywords and one or more arguments for that
keyword on a line.

| Keyword          | Arguments           | Description                                      |
| ---------------- | ------------------- |------------------------------------------------- |
| `vjm-version`    | \<major\>.\<minor\> | VJM version number in **major**.**minor** format |
| `device-name`    | "\<device name\>"   | Device name string                               |
| `device-vendor`  | \<vendor ID\>       | Unsigned 16-bit USB vendor ID                    |
| `device-product` | \<product ID\>      | Unsigned 16-bit USB product ID                   |
| `pin`            |                     | Map input to joystick pin                        |
| `pot`            |                     | Map input to potentiometer                       |
| `key`            |                     | Map input to key press                           |
| `action`         |                     | Map input to UI action                           |
