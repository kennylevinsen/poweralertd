#ifndef _UPOWER_H
#define _UPOWER_H

#include <systemd/sd-bus.h>
#include "list.h"

// org.freedesktop.UPower.Device.State
// https://upower.freedesktop.org/docs/Device.html
enum upower_state {
	state_unknown = 0,
	state_charging = 1,
	state_discharging = 2,
	state_empty = 3,
	state_fully_charged = 4,
	state_pending_charge = 5,
	state_pending_discharge = 6,
};

// org.freedesktop.UPower.Device.WarningLevel
// https://upower.freedesktop.org/docs/Device.html
enum upower_warning_level {
	warning_level_unknown = 0,
	warning_level_none = 1,
	warning_level_discharging = 2,
	warning_level_low = 3,
	warning_level_critical = 4,
	warning_level_action = 5,
};

// org.freedesktop.UPower.Device.Type
// https://upower.freedesktop.org/docs/Device.html
enum upower_type {
	type_unknown = 0,
	type_line_power = 1,
	type_battery = 2,
	type_ups = 3,
	type_monitor = 4,
	type_mouse = 5,
	type_keyboard = 6,
	type_pda = 7,
	type_phone = 8,
};

struct upower_device {
	char* path;

	int power_supply;
	enum upower_type type;
	double percentage;
	enum upower_state state;
	enum upower_warning_level warning_level;
	char *model;

	int state_changed;
	int warning_level_changed;

	int live;
	sd_bus_slot *slot;
};

struct power_state {
	list_t *devices;
	sd_bus *bus;
};

int init_upower(sd_bus *bus, struct power_state *state);
void destroy_upower(sd_bus *bus, struct power_state *state);

#endif