#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dbus.h"
#include "notify.h"
#include "upower.h"
#include "list.h"

#define NOTIFICATION_MAX_LEN 128

static int send_remove(sd_bus *bus, struct upower_device *device) {
	enum urgency urgency = URGENCY_NORMAL;
	char title[NOTIFICATION_MAX_LEN];

	if (strlen(device->model) > 0) {
		snprintf(title, NOTIFICATION_MAX_LEN, "Power status: %s", device->model);
	} else {
		snprintf(title, NOTIFICATION_MAX_LEN, "Power status: %s (%s)", device->native_path, upower_device_type_string(device));
	}
	char *msg = "Device disconnected\n";

	return notify(bus, title, msg, 0, urgency);
}

static int send_online_update(sd_bus *bus, struct upower_device *device) {
	if (device->current.online == device->last.online) {
		return 0;
	}

	char title[NOTIFICATION_MAX_LEN];
	char *msg;

	if (strlen(device->model) > 0) {
		snprintf(title, NOTIFICATION_MAX_LEN, "Power status: %s", device->model);
	} else {
		snprintf(title, NOTIFICATION_MAX_LEN, "Power status: %s (%s)", device->native_path, upower_device_type_string(device));
	}
	if (device->current.online) {
		msg = "Power supply online";
	} else {
		msg = "Power supply offline";
	}

	return notify(bus, title, msg, &device->notifications[SLOT_ONLINE], URGENCY_NORMAL);
}

static int send_state_update(sd_bus *bus, struct upower_device *device) {
	if (device->current.state == device->last.state) {
		return 0;
	}

	enum urgency urgency;
	char title[NOTIFICATION_MAX_LEN];
	char msg[NOTIFICATION_MAX_LEN];

	switch (device->current.state) {
	case UPOWER_DEVICE_STATE_UNKNOWN:
		// Silence transitions to/from unknown
		device->current.state = device->last.state;
		return 0;
	case UPOWER_DEVICE_STATE_EMPTY:
	case UPOWER_DEVICE_STATE_PENDING_CHARGE:
		urgency = URGENCY_CRITICAL;
		break;
	default:
		urgency = URGENCY_NORMAL;
		break;
	}

	if (strlen(device->model) > 0) {
		snprintf(title, NOTIFICATION_MAX_LEN, "Power status: %s", device->model);
	} else {
		snprintf(title, NOTIFICATION_MAX_LEN, "Power status: %s (%s)", device->native_path, upower_device_type_string(device));
	}
	if (device->current.battery_level != UPOWER_DEVICE_LEVEL_NONE) {
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery %s\nCurrent level: %s\n", upower_device_state_string(device), upower_device_battery_level_string(device));
	} else {
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery %s\nCurrent level: %0.0lf%%\n", upower_device_state_string(device), device->current.percentage);
	}

	return notify(bus, title, msg, &device->notifications[SLOT_STATE], urgency);
}

static int send_warning_update(sd_bus *bus, struct upower_device *device) {
	if (device->current.warning_level == device->last.warning_level) {
		return 0;
	}

	enum urgency urgency = URGENCY_CRITICAL;
	char title[NOTIFICATION_MAX_LEN];
	char *msg;

	switch (device->current.warning_level) {
	case UPOWER_DEVICE_LEVEL_NONE:
		msg = "Warning cleared\n";
		urgency = URGENCY_NORMAL;
		break;
	case UPOWER_DEVICE_LEVEL_DISCHARGING:
		msg = "Warning: system discharging\n";
		break;
	case UPOWER_DEVICE_LEVEL_LOW:
		msg = "Warning: power level low\n";
		break;
	case UPOWER_DEVICE_LEVEL_CRITICAL:
		msg = "Warning: power level critical\n";
		break;
	case UPOWER_DEVICE_LEVEL_ACTION:
		msg = "Warning: power level at action threshold\n";
		break;
	default:
		msg = "Warning: unknown warning level\n";
		break;
	}

	if (strlen(device->model) > 0) {
		snprintf(title, NOTIFICATION_MAX_LEN, "Power warning: %s", device->model);
	} else {
		snprintf(title, NOTIFICATION_MAX_LEN, "Power warning: %s (%s)", device->native_path, upower_device_type_string(device));
	}

	return notify(bus, title, msg, &device->notifications[SLOT_WARNING], urgency);
}

int main(int argc, char *argv[]) {
	struct upower state = { 0 };
	sd_bus *user_bus = NULL;
	sd_bus *system_bus = NULL;
	int ret;

	ret = sd_bus_open_user(&user_bus);
	if (ret < 0) {
		fprintf(stderr, "could not connect to session bus: %s\n", strerror(-ret));
		goto finish;
	}

	ret = sd_bus_open_system(&system_bus);
	if (ret < 0) {
		fprintf(stderr, "could not connect to system bus: %s\n", strerror(-ret));
		goto finish;
	}

	state.bus = system_bus;

	ret = init_upower(system_bus, &state);
	if (ret < 0) {
		fprintf(stderr, "could not init upower: %s\n", strerror(-ret));
		goto finish;
	}

	while (1) {
		for (int idx = 0; idx < state.devices->length; idx++) {
			struct upower_device *device = state.devices->items[idx];

			if (upower_device_has_battery(device)) {
				ret = send_state_update(user_bus, device);
				if (ret < 0) {
					fprintf(stderr, "could not send state update notification: %s\n", strerror(-ret));
					goto finish;
				}
				ret = send_warning_update(user_bus, device);
				if (ret < 0) {
					fprintf(stderr, "could not send warning update notification: %s\n", strerror(-ret));
					goto finish;
				}
			} else {
				ret = send_online_update(user_bus, device);
				if (ret < 0) {
					fprintf(stderr, "could not send online update notification: %s\n", strerror(-ret));
					goto finish;
				}
			}

			device->last = device->current;
		}

		for (int idx = 0; idx < state.removed_devices->length; idx++) {
			struct upower_device *device = state.removed_devices->items[idx];

			ret = send_remove(user_bus, device);
			if (ret < 0) {
				fprintf(stderr, "could not send device removal notification: %s\n", strerror(-ret));
				goto finish;
			}
			upower_device_destroy(device);
			list_del(state.removed_devices, idx);
		}

		ret = sd_bus_process(system_bus, NULL);
		if (ret < 0) {
			fprintf(stderr, "could not process system bus messages: %s\n", strerror(-ret));
			goto finish;
		} else if (ret > 0) {
			continue;
		}


		ret = sd_bus_wait(system_bus, UINT64_MAX);
		if (ret < 0) {
			fprintf(stderr, "could not wait for system bus messages: %s\n", strerror(-ret));
			goto finish;
		}
	}

finish:
	destroy_upower(system_bus, &state);
	sd_bus_unref(user_bus);
	sd_bus_unref(system_bus);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
