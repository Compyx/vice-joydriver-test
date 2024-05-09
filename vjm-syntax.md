# VICE joymap file syntax

## General syntax

Integer values can be specified in decimal, binary or hexadecimal notation.
Without any prefix integer literals are interpreted as decimal, with a **`0b`**,
**`0B`** or **`%`** prefix integer literals are treated as binary, and with a
**`0x`**, **`0X`** or **`$`** integeral literals are treated as hexadecimal.

String literals with spaces in them can be specified using double quotation
marks (**`"`**) and double quotation marks which should be part of a string
can be escaped with **`\`** (e.g. "A \"quoted\" word").

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
| `map`            | (variable)          | Declare mapping of host input to emulated input  |
| `pin`            |                     | Map input to joystick pin                        |
| `pot`            |                     | Map input to potentiometer (**TODO**)            |
| `key`            |                     | Map input to key press                           |
| `action`         |                     | Map input to UI action                           |
| `negative`       |                     | Negative axis direction (usually up or left)     |
| `positive`       |                     | Positive axis direction (usually down or right)  |
| `up`             |                     | Hat up direction                                 |
| `down`           |                     | Hat down direction                               |
| `left`           |                     | Hat left direction                               |
| `right`          |                     | Hat right direction                              |
| `inverted`       |                     | Host input value should be inverted              |
