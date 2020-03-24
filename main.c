#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include "notify.h"
#include "upower.h"
#include "list.h"

#define NOTIFICATION_MAX_LEN 128

static int send_state_update(sd_bus *bus, struct upower_device *device) {
	enum urgency urgency = urgency_normal;
	char title[NOTIFICATION_MAX_LEN];
	char msg[NOTIFICATION_MAX_LEN];
	switch (device->state) {
	case state_charging:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery charging\nCurrent level: %0.0lf%%\n", device->percentage);
		break;
	case state_discharging:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery discharging\nCurrent level: %0.0lf%%\n", device->percentage);
		break;
	case state_empty:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery empty\nCurrent level: %0.0lf%%\n", device->percentage);
		urgency = urgency_critical;
		break;
	case state_fully_charged:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery fully charged\nCurrent level: %0.0lf%%\n", device->percentage);
		break;
	case state_pending_charge:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery pending charge\nCurrent level: %0.0lf%%\n", device->percentage);
		urgency = urgency_critical;
		break;
	case state_pending_discharge:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery pending discharge\nCurrent level: %0.0lf%%\n", device->percentage);
		break;
	case state_unknown:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Unknown power state\nCurrent level: %0.0lf%%\n", device->percentage);
		urgency = urgency_critical;
		break;
	}

	snprintf(title, NOTIFICATION_MAX_LEN, "Power status: %s", device->model);

	return notify(bus, title, msg, 0, urgency);
}

static int send_warning_update(sd_bus *bus, struct upower_device *device) {
	enum urgency urgency = urgency_critical;
	char title[NOTIFICATION_MAX_LEN];
	char *msg = NULL;
	switch (device->warning_level) {
	case warning_level_none:
		msg = "Warning cleared\n";
		urgency = urgency_normal;
		break;
	case warning_level_discharging:
		msg = "Warning: system discharging\n";
		break;
	case warning_level_low:
		msg = "Warning: power level low\n";
		break;
	case warning_level_critical:
		msg = "Warning: power level critical\n";
		break;
	case warning_level_action:
		msg = "Warning: power level at action threshold\n";
		break;
	case warning_level_unknown:
		msg = "Warning: unknown warning level\n";
		break;
	}

	snprintf(title, NOTIFICATION_MAX_LEN, "Power warning: %s", device->model);

	return notify(bus, title, msg, 0, urgency);
}

int main(int argc, char *argv[]) {
	struct power_state state = { 0 };
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

			if (device->type != type_line_power && device->state_changed) {
				ret = send_state_update(user_bus, device);
				if (ret < 0) {
					fprintf(stderr, "could not send state update notification: %s\n", strerror(-ret));
					goto finish;
				}
				device->state_changed = 0;
			}
			if (device->warning_level_changed) {
				ret = send_warning_update(user_bus, device);
				if (ret < 0) {
					fprintf(stderr, "could not send warning update notification: %s\n", strerror(-ret));
					goto finish;
				}
				device->warning_level_changed = 0;
			}
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