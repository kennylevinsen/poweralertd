#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "dbus.h"
#include "upower.h"

static char *upower_state_string[UPOWER_DEVICE_STATE_LAST] = {
	"unknown",
	"charging",
	"discharging",
	"empty",
	"fully charged",
	"pending charge",
	"pending discharge",
};

static char *upower_level_string[UPOWER_DEVICE_LEVEL_LAST] = {
	"unknown",
	"none",
	"discharging",
	"low",
	"critical",
	"action",
	"normal",
	"high",
	"full"
};

static char *upower_type_string[UPOWER_DEVICE_TYPE_LAST] = {
	"unknown",
	"line power",
	"battery",
	"ups",
	"monitor",
	"mouse",
	"keyboard",
	"pda",
	"phone",
	"media player",
	"tablet",
	"computer",
	"gaming input",
	"pen",
	"touchpad",
	"modem",
	"network",
	"headset",
	"speakers",
	"headphones",
	"video",
	"other audio",
	"remote control",
	"printer",
	"scanner",
	"camera",
	"wearable",
	"toy",
	"bluetooth generic",
};

int upower_device_has_battery(struct upower_device *device) {
	return device->type != UPOWER_DEVICE_TYPE_LINE_POWER && device->type != UPOWER_DEVICE_TYPE_UNKNOWN;
}

static int upower_compare_path(const void *item, const void *data) {
	struct upower_device *device = (struct upower_device *)item;
	if (strcmp(device->path, (char*)data) == 0) {
		return 0;
	}
	return -1;
}

static struct upower_device *upower_device_create() {
	struct upower_device *device = calloc(1, sizeof(struct upower_device));
	device->last.warning_level = UPOWER_DEVICE_LEVEL_NONE;
	device->current.warning_level = UPOWER_DEVICE_LEVEL_NONE;
	device->last.battery_level = UPOWER_DEVICE_LEVEL_NONE;
	device->current.battery_level = UPOWER_DEVICE_LEVEL_NONE;
	return device;
}

void upower_device_destroy(struct upower_device *device) {
	if (device == NULL) {
		return;
	}

	if (device->path != NULL) {
		free(device->path);
		device->path = NULL;
	}
	if (device->native_path != NULL) {
		free(device->native_path);
		device->native_path = NULL;
	}
	if (device->model != NULL) {
		free(device->model);
		device->model = NULL;
	}
	if (device->slot != NULL) {
		sd_bus_slot_unref(device->slot);
		device->slot = NULL;
	}
	free(device);
}

char* upower_device_state_string(struct upower_device *device) {
	if (device->current.state >= 0 && device->current.state < UPOWER_DEVICE_STATE_LAST) {
		return upower_state_string[device->current.state];
	}
	return "unknown";
}

char* upower_device_warning_level_string(struct upower_device *device) {
	if (device->current.warning_level >= 0 && device->current.warning_level < UPOWER_DEVICE_LEVEL_LAST) {
		return upower_level_string[device->current.warning_level];
	}
	return "unknown";
}

char* upower_device_battery_level_string(struct upower_device *device) {
	if (device->current.battery_level >= 0 && device->current.battery_level < UPOWER_DEVICE_LEVEL_LAST) {
		return upower_level_string[device->current.battery_level];
	}
	return "unknown";
}

char* upower_device_type_string(struct upower_device *device) {
	if (device->type >= 0 && device->type < UPOWER_DEVICE_TYPE_LAST) {
		return upower_type_string[device->type];
	}
	return "unknown";
}

int upower_device_type_int(char *device) {
	for (int i=0; i < UPOWER_DEVICE_TYPE_LAST; i++) {
		if (!strcmp(upower_type_string[i], device)) {
			return i;
		}
	}
	return -1;
}

static int upower_device_update_state(sd_bus *bus, struct upower_device *device) {
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int ret;

	char *native_path = NULL;
	ret = sd_bus_get_property_string(
	    bus,
	    "org.freedesktop.UPower",
	    device->path,
	    "org.freedesktop.UPower.Device",
	    "NativePath",
	    &error,
	    &native_path);
	if (ret < 0) {
		goto finish;
	}
	if (device->native_path != NULL) {
		free(device->native_path);
	}
	device->native_path = strdup(native_path);

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
	if (device->model != NULL) {
		free(device->model);
	}
	device->model = strdup(model);

	ret = sd_bus_get_property_trivial(
	    bus,
	    "org.freedesktop.UPower",
	    device->path,
	    "org.freedesktop.UPower.Device",
	    "Online",
	    &error,
	    'b',
	    &device->current.online);
	if (ret < 0) {
		goto finish;
	}

	ret = sd_bus_get_property_trivial(
	    bus,
	    "org.freedesktop.UPower",
	    device->path,
	    "org.freedesktop.UPower.Device",
	    "State",
	    &error,
	    'u',
	    &device->current.state);
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
	    &device->current.warning_level);
	if (ret < 0) {
		goto finish;
	}

	ret = sd_bus_get_property_trivial(
	    bus,
	    "org.freedesktop.UPower",
	    device->path,
	    "org.freedesktop.UPower.Device",
	    "BatteryLevel",
	    &error,
	    'u',
	    &device->current.battery_level);
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
	    &device->current.percentage);

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
			ret = sd_bus_message_read(msg, "v", "u", &state->current.state);
			if (ret < 0) {
				goto error;
			}
		} else if (strcmp(name, "WarningLevel") == 0) {
			ret = sd_bus_message_read(msg, "v", "u", &state->current.warning_level);
			if (ret < 0) {
				goto error;
			}
		} else if (strcmp(name, "BatteryLevel") == 0) {
			ret = sd_bus_message_read(msg, "v", "u", &state->current.battery_level);
			if (ret < 0) {
				goto error;
			}
		} else if (strcmp(name, "Online") == 0) {
			ret = sd_bus_message_read(msg, "v", "b", &state->current.online);
			if (ret < 0) {
				goto error;
			}
		} else if (strcmp(name, "Percentage") == 0) {
			ret = sd_bus_message_read(msg, "v", "d", &state->current.percentage);
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
	struct upower *state = userdata;
	struct upower_device *device;
	int ret, idx;

	char *path;
	ret = sd_bus_message_read(msg, "o", &path);
	if (ret < 0) {
		goto error;
	}

	// Look for doubly-added devices
	idx = list_seq_find(state->devices, upower_compare_path, path);
	if (idx != -1) {
		device = state->devices->items[idx];
		goto update;
	}

	// Look for recently removed devices
	idx = list_seq_find(state->removed_devices, upower_compare_path, path);
	if (idx != -1) {
		device = state->removed_devices->items[idx];
		list_add(state->devices, device);
		list_del(state->removed_devices, idx);
		goto update;
	}

	// Fresh device
	device = calloc(1, sizeof(struct upower_device));
	device->path = strdup(path);

	list_add(state->devices, device);

	ret = upower_device_register_notification(state->bus, device);
	if (ret < 0) {
		goto error;
	}

update:
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
	struct upower *state = userdata;
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

int init_upower(sd_bus *bus, struct upower *state) {
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

void destroy_upower(sd_bus *bus, struct upower *state) {
	if (state->devices != NULL) {
		for (int idx = 0; idx < state->devices->length; idx++) {
			upower_device_destroy(state->devices->items[idx]);
		}
		list_free(state->devices);
		state->devices = NULL;
	}
}
