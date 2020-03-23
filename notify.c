#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include "notify.h"

int notify(sd_bus *bus, char *summary, char *body, uint32_t id, enum urgency urgency) {
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL;
	int ret = sd_bus_call_method(bus,
	    "org.freedesktop.Notifications",
	    "/org/freedesktop/Notifications",
	    "org.freedesktop.Notifications",
	    "Notify",
	    &error,
	    &m,
	    "susssasa{sv}i",
	    "poweralertd",
	    id,
	    "",
	    summary,
	    body,
	    0,
	    1,
	    "urgency", "y", (uint8_t)urgency,
	    -1);

	sd_bus_error_free(&error);
	sd_bus_message_unref(m);

	return ret;
}
