/** \file   config.h
 * \brief   Settings shared by the binaries
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef CONFIG_H
#define CONFIG_H

#define PROGRAM_NAME    "VICE joystick driver tester"
#define PROGRAM_VERSION "0.1"

#ifdef USE_SDL
#define DRIVER "SDL2"
#endif

#if defined(WINDOWS_COMPILE)
#define OSNAME "Windows"
#ifndef USE_SDL
#define DRIVER "DirectInput"
#endif

#elif defined(LINUX_COMPILE)
#define OSNAME "Linux"
#ifndef USE_SDL
#define DRIVER "evdev"
#endif

#elif defined(FREEBSD_COMPILE)
#define OSNAME "FreeBSD"
#ifndef USE_SDL
#define DRIVER "usbhid"
#endif

#elif defined(NETBSD_COMPILE)
#define OSNAME "NetBSD"
#ifndef USE_SDL
#define DRIVER "usbhid"
#endif

#elif defined(MACOS_COMPILE)
#define OSNAME "MacOS"
#ifndef USE_SDL
#define DRIVER "Unknown"
#endif

#else
#define OSNAME "Unknown"
#ifndef USE_SDL
#define DRIVER "Unknown"
#endif

#endif

#endif
