#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include "notify.h"
#include "upower.h"

#define NOTIFICATION_MAX_LEN 128

static int send_state_update(sd_bus *bus, struct power_state *state) {
	enum urgency urgency = urgency_normal;
	char *msg = malloc(NOTIFICATION_MAX_LEN);
	int ret;
	switch (state->state) {
	case state_charging:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery charging\nCurrent level: %0.0lf%%\n", state->percentage);
		break;
	case state_discharging:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery discharging\nCurrent level: %0.0lf%%\n", state->percentage);
		break;
	case state_empty:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery empty\nCurrent level: %0.0lf%%\n", state->percentage);
		urgency = urgency_critical;
		break;
	case state_fully_charged:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery fully charged\nCurrent level: %0.0lf%%\n", state->percentage);
		break;
	case state_pending_charge:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery pending charge\nCurrent level: %0.0lf%%\n", state->percentage);
		urgency = urgency_critical;
		break;
	case state_pending_discharge:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Battery pending discharge\nCurrent level: %0.0lf%%\n", state->percentage);
		break;
	case state_unknown:
		snprintf(msg, NOTIFICATION_MAX_LEN, "Unknown power state\nCurrent level: %0.0lf%%\n", state->percentage);
		urgency = urgency_critical;
		break;
	}

	ret = notify(bus, "Power state", msg, 1, urgency);
	free(msg);
	return ret;
}

static int send_warning_update(sd_bus *bus, struct power_state *state) {
	enum urgency urgency = urgency_critical;
	char *msg = NULL;
	switch (state->warning_level) {
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

	int ret = notify(bus, "Power warning", msg, 1, urgency);
	return ret;
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

	ret = register_upower_notification(system_bus, &state);
	if (ret < 0) {
		fprintf(stderr, "could not set up monitor: %s\n", strerror(-ret));
		goto finish;
	}

	ret = upower_state_update(system_bus, &state);
	if (ret < 0) {
		fprintf(stderr, "could not read initial state: %s\n", strerror(-ret));
		goto finish;
	}

	// Send start-up state change message
	ret = send_state_update(user_bus, &state);
	if (ret < 0) {
		fprintf(stderr, "could not send initial update notification: %s\n", strerror(-ret));
		goto finish;
	}
	state.state_changed = 0;

	while (1) {
		ret = sd_bus_process(system_bus, NULL);
		if (ret < 0) {
			fprintf(stderr, "could not process system bus messages: %s\n", strerror(-ret));
			goto finish;
		} else if (ret > 0) {
			continue;
		}

		if (state.state_changed) {
			ret = send_state_update(user_bus, &state);
			if (ret < 0) {
				fprintf(stderr, "could not send state update notification: %s\n", strerror(-ret));
				goto finish;
			}
			state.state_changed = 0;
		}
		if (state.warning_level_changed) {
			ret = send_warning_update(user_bus, &state);
			if (ret < 0) {
				fprintf(stderr, "could not send warning update notification: %s\n", strerror(-ret));
				goto finish;
			}
			state.warning_level_changed = 0;
		}

		ret = sd_bus_wait(system_bus, UINT64_MAX);
		if (ret < 0) {
			fprintf(stderr, "could not wait for system bus messages: %s\n", strerror(-ret));
			goto finish;
		}
	}

finish:
	sd_bus_unref(user_bus);
	sd_bus_unref(system_bus);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}