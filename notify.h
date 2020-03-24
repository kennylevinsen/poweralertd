#ifndef _NOTIFY_H
#define _NOTIFY_H

#include "dbus.h"

// Urgency values to be used as hint in org.freedesktop.Notifications.Notify calls.
// https://people.gnome.org/~mccann/docs/notification-spec/notification-spec-latest.html#hints
enum urgency {
	urgency_low = 0,
	urgency_normal = 1,
	urgency_critical = 2,
};

int notify(sd_bus *bus, char *summary, char *body, uint32_t *id, enum urgency urgency);

#endif