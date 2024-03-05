#ifndef _UPOWER_H
#define _UPOWER_H

#include "dbus.h"
#include "list.h"

// org.freedesktop.UPower.Device.State
// https://upower.freedesktop.org/docs/Device.html
enum upower_device_state {
	UPOWER_DEVICE_STATE_UNKNOWN,
	UPOWER_DEVICE_STATE_CHARGING,
	UPOWER_DEVICE_STATE_DISCHARGING,
	UPOWER_DEVICE_STATE_EMPTY,
	UPOWER_DEVICE_STATE_FULLY_CHARGED,
	UPOWER_DEVICE_STATE_PENDING_CHARGE,
	UPOWER_DEVICE_STATE_PENDING_DISCHARGE,
	UPOWER_DEVICE_STATE_LAST
};

// org.freedesktop.UPower.Device.WarningLevel
// https://upower.freedesktop.org/docs/Device.html
enum upower_device_level {
	UPOWER_DEVICE_LEVEL_UNKNOWN,
	UPOWER_DEVICE_LEVEL_NONE,
	UPOWER_DEVICE_LEVEL_DISCHARGING,
	UPOWER_DEVICE_LEVEL_LOW,
	UPOWER_DEVICE_LEVEL_CRITICAL,
	UPOWER_DEVICE_LEVEL_ACTION,
	UPOWER_DEVICE_LEVEL_NORMAL,
	UPOWER_DEVICE_LEVEL_HIGH,
	UPOWER_DEVICE_LEVEL_FULL,
	UPOWER_DEVICE_LEVEL_LAST

};

// org.freedesktop.UPower.Device.Type
// https://upower.freedesktop.org/docs/Device.html
enum upower_device_type {
	UPOWER_DEVICE_TYPE_UNKNOWN,
	UPOWER_DEVICE_TYPE_LINE_POWER,
	UPOWER_DEVICE_TYPE_BATTERY,
	UPOWER_DEVICE_TYPE_UPS,
	UPOWER_DEVICE_TYPE_MONITOR,
	UPOWER_DEVICE_TYPE_MOUSE,
	UPOWER_DEVICE_TYPE_KEYBOARD,
	UPOWER_DEVICE_TYPE_PDA,
	UPOWER_DEVICE_TYPE_PHONE,
	UPOWER_DEVICE_TYPE_MEDIA_PLAYER,
	UPOWER_DEVICE_TYPE_TABLET,
	UPOWER_DEVICE_TYPE_COMPUTER,
	UPOWER_DEVICE_TYPE_GAMING_INPUT,
	UPOWER_DEVICE_TYPE_PEN,
	UPOWER_DEVICE_TYPE_TOUCHPAD,
	UPOWER_DEVICE_TYPE_MODEM,
	UPOWER_DEVICE_TYPE_NETWORK,
	UPOWER_DEVICE_TYPE_HEADSET,
	UPOWER_DEVICE_TYPE_SPEAKERS,
	UPOWER_DEVICE_TYPE_HEADPHONES,
	UPOWER_DEVICE_TYPE_VIDEO,
	UPOWER_DEVICE_TYPE_OTHER_AUDIO,
	UPOWER_DEVICE_TYPE_REMOTE_CONTROL,
	UPOWER_DEVICE_TYPE_PRINTER,
	UPOWER_DEVICE_TYPE_SCANNER,
	UPOWER_DEVICE_TYPE_CAMERA,
	UPOWER_DEVICE_TYPE_WEARABLE,
	UPOWER_DEVICE_TYPE_TOY,
	UPOWER_DEVICE_TYPE_BLUETOOTH_GENERIC,
	UPOWER_DEVICE_TYPE_LAST
};

enum change_slot {
	SLOT_STATE = 0,
	SLOT_WARNING = 1,
	SLOT_ONLINE = 2,
};

struct upower_device_props {
	int generation;
	int online;
	double percentage;
	enum upower_device_state state;
	enum upower_device_level warning_level;
	enum upower_device_level battery_level;
};

struct upower_device {
	// Static properties
	char* path;
	char* native_path;
	char *model;
	int power_supply;
	enum upower_device_type type;

	// Monitored properties
	struct upower_device_props current;
	struct upower_device_props last;

	// Property notification
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
char* upower_device_battery_level_string(struct upower_device *device);
char* upower_device_type_string(struct upower_device *device);
int upower_device_type_int(char *device);
void upower_device_destroy(struct upower_device *device);

int init_upower(sd_bus *bus, struct upower *state);
void destroy_upower(sd_bus *bus, struct upower *state);

#endif
