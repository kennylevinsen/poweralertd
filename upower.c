#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include "upower.h"

static char *upower_state_string[7] = {
	"unknown",
	"charging",
	"discharging",
	"empty",
	"fully charged",
	"pending charge",
	"pending discharge",
};

static char *upower_warning_level_string[6] = {
	"unknown",
	"none",
	"discharging",
	"low",
	"critical",
	"action",
};

static int upower_compare_path(const void *item, const void *data) {
	struct upower_device *device = (struct upower_device *)item;
	if (strcmp(device->path, (char*)data) == 0) {
		return 0;
	}
	return -1;
}

static struct upower_device *upower_device_create() {
	return calloc(1, sizeof(struct upower_device));
}

void upower_device_destroy(struct upower_device *device) {
	if (device == NULL) {
		return;
	}

	if (device->path != NULL) {
		free(device->path);
		device->path = NULL;
	}
	if (device->model != NULL) {
		free(device->model);
		device->model = NULL;
	}
	free(device);
}

char* upower_device_state_string(struct upower_device *device) {
	if (device->state >= 0 && device->state < 7) {
		return upower_state_string[device->state];
	}
	return "unknown";
}

char* upower_device_warning_level_string(struct upower_device *device) {
	if (device->warning_level >= 0 && device->warning_level < 7) {
		return upower_warning_level_string[device->warning_level];
	}
	return "unknown";
}

static int upower_device_update_state(sd_bus *bus, struct upower_device *device) {
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int ret;

	ret = sd_bus_get_property_trivial(
	    bus,
	    "org.freedesktop.UPower",
	    device->path,
	    "org.freedesktop.UPower.Device",
	    "PowerSupply",
	    &error,
	    'b',
	    &device->power_supply);
	if (ret < 0) {
		goto finish;
	}

	ret = sd_bus_get_property_trivial(
	    bus,
	    "org.freedesktop.UPower",
	    device->path,
	    "org.freedesktop.UPower.Device",
	    "Type",
	    &error,
	    'u',
	    &device->type);
	if (ret < 0) {
		goto finish;
	}

	char *model = NULL;
	ret = sd_bus_get_property_string(
	    bus,
	    "org.freedesktop.UPower",
	    device->path,
	    "org.freedesktop.UPower.Device",
	    "Model",
	    &error,
	    &model);
	if (ret < 0) {
		goto finish;
	}
	device->model = strdup(model);

	ret = sd_bus_get_property_trivial(
	    bus,
	    "org.freedesktop.UPower",
	    device->path,
	    "org.freedesktop.UPower.Device",
	    "State",
	    &error,
	    'u',
	    &device->state);
	if (ret < 0) {
		goto finish;
	}

	ret = sd_bus_get_property_trivial(
	    bus,
	    "org.freedesktop.UPower",
	    device->path,
	    "org.freedesktop.UPower.Device",
	    "WarningLevel",
	    &error,
	    'u',
	    &device->warning_level);
	if (ret < 0) {
		goto finish;
	}

	ret = sd_bus_get_property_trivial(
	    bus,
	    "org.freedesktop.UPower",
	    device->path,
	    "org.freedesktop.UPower.Device",
	    "Percentage",
	    &error,
	    'd',
	    &device->percentage);

	device->state_changed = 1;

finish:
	sd_bus_error_free(&error);
	return ret;
}
static int handle_upower_device_properties_changed(sd_bus_message *msg, void *userdata, sd_bus_error *ret_error) {
	struct upower_device *state = userdata;
	int ret;

	ret = sd_bus_message_skip(msg, "s");
	if (ret < 0) {
		goto error;
	}

	ret = sd_bus_message_enter_container(msg, 'a', "{sv}");
	if (ret < 0) {
		goto error;
	}

	while (1) {
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
			uint32_t new_state;
			ret = sd_bus_message_read(msg, "v", "u", &new_state);
			if (ret < 0) {
				goto error;
			}
			if (new_state != state->state) {
				state->state_changed = 1;
				state->state = new_state;
			}
		} else if (strcmp(name, "WarningLevel") == 0) {
			uint32_t new_warning_level;
			ret = sd_bus_message_read(msg, "v", "u", &new_warning_level);
			if (ret < 0) {
				goto error;
			}
			if (new_warning_level != state->warning_level) {
				state->warning_level_changed = 1;
				state->warning_level = new_warning_level;
			}
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

	while (1) {
		ret = sd_bus_message_skip(msg, "s");
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
	fprintf(stderr, "handle_upower_device_properties_changed failed: %s\n", strerror(-ret));
	return ret;
}

static int upower_device_register_notification(sd_bus *bus, struct upower_device *device) {
	char match[512];
	snprintf(match, 512, "type='signal',path='%s',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'", device->path);

	return sd_bus_add_match(bus, &device->slot, match, handle_upower_device_properties_changed, device);
}

static int handle_upower_device_added(sd_bus_message *msg, void *userdata, sd_bus_error *ret_error) {
	struct power_state *state = userdata;
	int ret;

	struct upower_device *device = calloc(1, sizeof(struct upower_device));
	list_add(state->devices, device);

	char *path;
	ret = sd_bus_message_read(msg, "o", &path);
	if (ret < 0) {
		goto error;
	}

	device->path = strdup(path);
	ret = upower_device_register_notification(state->bus, device);
	if (ret < 0) {
		goto error;
	}
	ret = upower_device_update_state(state->bus, device);
	if (ret < 0) {
		goto error;
	}

	return 0;

error:
	fprintf(stderr, "handle_upower_device_added failed: %s\n", strerror(-ret));
	return ret;
}

static int handle_upower_device_removed(sd_bus_message *msg, void *userdata, sd_bus_error *ret_error) {
	struct power_state *state = userdata;
	int ret;

	char *path;
	ret = sd_bus_message_read(msg, "o", &path);
	if (ret < 0) {
		goto error;
	}

	int idx = list_seq_find(state->devices, upower_compare_path, path);
	if (idx != -1) {
		list_add(state->removed_devices, state->devices->items[idx]);
		list_del(state->devices, idx);
	}

	return 0;

error:
	fprintf(stderr, "handle_upower_device_removed failed: %s\n", strerror(-ret));
	return ret;
}

int init_upower(sd_bus *bus, struct power_state *state) {
	sd_bus_error error = SD_BUS_ERROR_NULL;
	sd_bus_message *msg = NULL;
	int ret;

	ret = sd_bus_add_match(
		bus,
		NULL,
		"type='signal',path='/org/freedesktop/UPower',interface='org.freedesktop.UPower',member='DeviceAdded'",
		handle_upower_device_added,
		state);

	if (ret < 0) {
		goto error;
	}

	ret = sd_bus_add_match(
		bus,
		NULL,
		"type='signal',path='/org/freedesktop/UPower',interface='org.freedesktop.UPower',member='DeviceRemoved'",
		handle_upower_device_removed,
		state);

	if (ret < 0) {
		goto error;
	}

	ret = sd_bus_call_method(bus,
	    "org.freedesktop.UPower",
	    "/org/freedesktop/UPower",
	    "org.freedesktop.UPower",
	    "EnumerateDevices",
	    &error,
	    &msg,
	    "");

	ret = sd_bus_message_enter_container(msg, 'a', "o");
	if (ret < 0) {
		goto error;
	}

	state->devices = create_list();
	state->removed_devices = create_list();

	while (1) {
		char *path;
		ret = sd_bus_message_read(msg, "o", &path);
		if (ret < 0) {
			goto error;
		} else if (ret == 0) {
			break;
		}

		struct upower_device *device = upower_device_create();
		list_add(state->devices, device);

		device->path = strdup(path);
		ret = upower_device_register_notification(bus, device);
		if (ret < 0) {
			goto error;
		}
		ret = upower_device_update_state(bus, device);
		if (ret < 0) {
			goto error;
		}
	}

	ret = sd_bus_message_exit_container(msg);
	if (ret < 0) {
		goto error;
	}

error:
	sd_bus_error_free(&error);
	sd_bus_message_unref(msg);

	return ret;
}

void destroy_upower(sd_bus *bus, struct power_state *state) {
	if (state->devices != NULL) {
		for (int idx = 0; idx < state->devices->length; idx++) {
			upower_device_destroy(state->devices->items[idx]);
		}
		list_free(state->devices);
		state->devices = NULL;
	}
}