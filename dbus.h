#ifndef _DBUS_H
#define _DBUS_H

#if defined(HAVE_BASU)
#include <basu/sd-bus.h>
#elif defined(HAVE_ELOGIND)
#include <elogind/sd-bus.h>
#elif defined(HAVE_SYSTEMD)
#include <systemd/sd-bus.h>
#endif

#endif
