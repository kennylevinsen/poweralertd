#ifndef _UPOWER_H
#define _UPOWER_H

#include <systemd/sd-bus.h>

enum upower_state {
	state_unknown = 0,
	state_charging = 1,
	state_discharging = 2,
	state_empty = 3,
	state_fully_charged = 4,
	state_pending_charge = 5,
	state_pending_discharge = 6,
};

enum upower_warning_level {
	warning_level_unknown = 0,
	warning_level_none = 1,
	warning_level_discharging = 2,
	warning_level_low = 3,
	warning_level_critical = 4,
	warning_level_action = 5,
};

struct power_state {
	double percentage;
	enum upower_state state;
	enum upower_warning_level warning_level;
	int state_changed;
	int warning_level_changed;
};

int register_upower_notification(sd_bus *bus, struct power_state *state);
int upower_state_update(sd_bus *bus, struct power_state *state);

#endif