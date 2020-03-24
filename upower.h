#ifndef _UPOWER_H
#define _UPOWER_H

#include "dbus.h"
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

enum change_slot {
	slot_state = 0,
	slot_warning = 1,
	slot_online = 2,
};

struct upower_device {
	// Path information
	char* path;
	char* native_path;

	// Props we're intersted in
	char *model;
	int power_supply;
	int online;
	double percentage;
	enum upower_type type;
	enum upower_state state;
	enum upower_warning_level warning_level;

	// Prop notification
	int changes[3];
	uint32_t notifications[3];

	// sd_bus notification slot
	sd_bus_slot *slot;
};

struct upower {
	list_t *devices;
	list_t *removed_devices;
	sd_bus *bus;
};

int upower_device_has_battery(struct upower_device *device);
char* upower_device_state_string(struct upower_device *device);
char* upower_device_warning_level_string(struct upower_device *device);
char* upower_device_type_string(struct upower_device *device);
void upower_device_destroy(struct upower_device *device);

int init_upower(sd_bus *bus, struct upower *state);
void destroy_upower(sd_bus *bus, struct upower *state);

#endif