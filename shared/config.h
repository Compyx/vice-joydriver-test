/** \file   config.h
 * \brief   Settings shared by the binaries
 * \author  Bas Wassink <b.wassink@ziggo.nl>
 */

#ifndef CONFIG_H
#define CONFIG_H

#define PROGRAM_NAME    "VICE joystick driver tester"
#define PROGRAM_VERSION "0.1"

#if defined(WINDOWS_COMPILE)
#define OSNAME "Windows"
#elif defined(LINUX_COMPILE)
#define OSNAME "Linux"
#elif defined(FREEBSD_COMPILE)
#define OSNAME "FreeBSD"
#elif defined(NETBSD_COMPILE)
#define OSNAME "NetBSD"
#elif defined(MACOS_COMPILE)
#define OSNAME "MacOS"
#else
#define OSNAME "Unknown"
#endif

#endif
