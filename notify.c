#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dbus.h"
#include "notify.h"

int notify(sd_bus *bus, char *summary, char *body, uint32_t *id, enum urgency urgency) {
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *msg = NULL;
	int ret = sd_bus_call_method(bus,
	    "org.freedesktop.Notifications",
	    "/org/freedesktop/Notifications",
	    "org.freedesktop.Notifications",
	    "Notify",
	    &error,
	    &msg,
	    "susssasa{sv}i",
	    "poweralertd",
	    id != NULL ? *id : 0,
	    "",
	    summary,
	    body,
	    0,
	    1,
	    "urgency", "y", (uint8_t)urgency,
	    -1);

	if (ret < 0) {
		goto error;
	}

	if (id != NULL) {
		ret = sd_bus_message_read(msg, "u", id);
		if (ret < 0) {
			goto error;
		}
	}

error:
	sd_bus_error_free(&error);
	sd_bus_message_unref(msg);

	return ret;
}
