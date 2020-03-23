#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include "upower.h"

static int handle_upower_properties_changed(sd_bus_message *msg, void *userdata, sd_bus_error *ret_error)
{
	struct power_state *state = userdata;

	int ret;
	ret = sd_bus_message_skip(msg, "s");
	if (ret < 0) {
		goto error;
	}

	ret = sd_bus_message_enter_container(msg, 'a', "{sv}");
	if (ret < 0) {
		goto error;
	}

	for (;;) {
		ret = sd_bus_message_enter_container(msg, 'e', "sv");
		if (ret < 0) {
			goto error;
		} else if (ret == 0) {
			break;
		}

		const char *name = NULL;
		ret = sd_bus_message_read(msg, "s", &name);
		if (ret < 0) {
			goto error;
		}
		if (strcmp(name, "State") == 0) {
			ret = sd_bus_message_read(msg, "v", "u", &state->state);
			if (ret < 0) {
				goto error;
			}
			state->state_changed = 1;
		} else if (strcmp(name, "WarningLevel") == 0) {
			ret = sd_bus_message_read(msg, "v", "u", &state->warning_level);
			if (ret < 0) {
				goto error;
			}
			state->warning_level_changed = 1;
		} else if (strcmp(name, "Percentage") == 0) {
			ret = sd_bus_message_read(msg, "v", "d", &state->percentage);
			if (ret < 0) {
				goto error;
			}
		} else {
			ret = sd_bus_message_skip(msg, "v");
			if (ret < 0) {
				goto error;
			}
		}

		ret = sd_bus_message_exit_container(msg);
		if (ret < 0) {
			goto error;
		}
	}

	ret = sd_bus_message_exit_container(msg);
	if (ret < 0) {
		goto error;
	}

	ret = sd_bus_message_enter_container(msg, 'a', "s");
	if (ret < 0) {
		goto error;
	}

	for (;;) {
		const char *invalidated = NULL;
		ret = sd_bus_message_read(msg, "s", &invalidated);
		if (ret < 0) {
			goto error;
		} else if (ret == 0) {
			break;
		}
	}

	ret = sd_bus_message_exit_container(msg);
	if (ret < 0) {
		goto error;
	}

	return 0;

error:
	fprintf(stderr, "handle_upower_properties_changed failed: %s\n", strerror(-ret));
	return ret;
}

int register_upower_notification(sd_bus *bus, struct power_state *state)
{
	char *match = "type='signal',path='/org/freedesktop/UPower/devices/DisplayDevice',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'";

	return sd_bus_add_match(bus, NULL, match, handle_upower_properties_changed, state);
}

int upower_state_update(sd_bus *bus, struct power_state *state)
{
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int ret;
	ret = sd_bus_get_property_trivial(
	    bus,
	    "org.freedesktop.UPower",
	    "/org/freedesktop/UPower/devices/DisplayDevice",
	    "org.freedesktop.UPower.Device",
	    "State",
	    &error,
	    'u',
	    &state->state);
	if (ret < 0) {
		goto finish;
	}
	ret = sd_bus_get_property_trivial(
	    bus,
	    "org.freedesktop.UPower",
	    "/org/freedesktop/UPower/devices/DisplayDevice",
	    "org.freedesktop.UPower.Device",
	    "WarningLevel",
	    &error,
	    'u',
	    &state->warning_level);
	if (ret < 0) {
		goto finish;
	}
	ret = sd_bus_get_property_trivial(
	    bus,
	    "org.freedesktop.UPower",
	    "/org/freedesktop/UPower/devices/DisplayDevice",
	    "org.freedesktop.UPower.Device",
	    "Percentage",
	    &error,
	    'd',
	    &state->percentage);

finish:
	sd_bus_error_free(&error);
	return ret;
}