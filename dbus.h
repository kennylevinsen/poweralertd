#ifndef _DBUS_H
#define _DBUS_H

#ifdef HAVE_SYSTEMD
#include <systemd/sd-bus.h>
#elif HAVE_ELOGIND
#include <elogind/sd-bus.h>
#endif

#endif