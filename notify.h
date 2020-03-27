#ifndef _NOTIFY_H
#define _NOTIFY_H

#include "dbus.h"

// Urgency values to be used as hint in org.freedesktop.Notifications.Notify calls.
// https://people.gnome.org/~mccann/docs/notification-spec/notification-spec-latest.html#hints
enum urgency {
	URGENCY_LOW,
	URGENCY_NORMAL,
	URGENCY_CRITICAL,
};

int notify(sd_bus *bus, char *summary, char *body, uint32_t *id, enum urgency urgency);

#endif