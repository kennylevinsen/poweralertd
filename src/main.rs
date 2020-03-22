use dbus::blocking::stdintf::org_freedesktop_dbus::Properties;
use std::cell::RefCell;
use std::collections::HashMap;
use std::error::Error;
use std::rc::Rc;
use std::time::Duration;

macro_rules! notify {
    ($conn:expr, $id:expr, $summary:expr, $body:expr, $urgency:expr) => {{
        let actions: [String; 0] = [];
        let mut hints: HashMap<String, dbus::arg::Variant<Box<dyn dbus::arg::RefArg>>> =
            HashMap::new();
        hints.insert(
            "urgency".to_string(),
            dbus::arg::Variant(Box::new($urgency) as Box<dyn dbus::arg::RefArg>),
        );
        $conn.method_call(
            "org.freedesktop.Notifications",
            "Notify",
            (
                "poweralertd",
                $id as u32,
                "",
                $summary,
                $body,
                dbus::arg::Array::new(&actions),
                dbus::arg::Dict::new(&hints),
                -1 as i32,
            ),
        )?;
    }};
}

fn refarg_as_u64(value: &dyn dbus::arg::RefArg) -> Option<u64> {
    value.as_u64()
}

fn refarg_as_f64(value: &dyn dbus::arg::RefArg) -> Option<f64> {
    value.as_f64()
}

#[derive(Debug)]
enum UPowerState {
    Charging,
    Discharging,
    Empty,
    Full,
    NotCharging,
    Unknown,
}

impl UPowerState {
    fn from_u64(v: u64) -> UPowerState {
        match v {
            1 => UPowerState::Charging,
            2 => UPowerState::Discharging,
            3 => UPowerState::Empty,
            4 => UPowerState::Full,
            5 => UPowerState::NotCharging,
            6 => UPowerState::Discharging,
            _ => UPowerState::Unknown,
        }
    }
}

#[derive(Debug)]
enum UPowerWarningLevel {
    Unknown,
    None,
    Discharging,
    Low,
    Critical,
    Action,
}

impl UPowerWarningLevel {
    fn from_u64(v: u64) -> UPowerWarningLevel {
        match v {
            1 => UPowerWarningLevel::None,
            2 => UPowerWarningLevel::Discharging,
            3 => UPowerWarningLevel::Low,
            4 => UPowerWarningLevel::Critical,
            5 => UPowerWarningLevel::Action,
            _ => UPowerWarningLevel::Unknown,
        }
    }
}

struct State {
    percentage: f64,
    state: UPowerState,
    warning_level: UPowerWarningLevel,
    state_changed: bool,
    warn_changed: bool,
}

fn run() -> Result<(), Box<dyn Error>> {
    let mut conn = dbus::blocking::LocalConnection::new_system()?;
    let display_device = conn.with_proxy(
        "org.freedesktop.UPower",
        "/org/freedesktop/UPower/devices/DisplayDevice",
        Duration::from_millis(10000),
    );
    let sess_conn = dbus::blocking::LocalConnection::new_session()?;
    let notifications = sess_conn.with_proxy(
        "org.freedesktop.Notifications",
        "/org/freedesktop/Notifications",
        Duration::from_millis(10000),
    );

    let state = Rc::new(RefCell::new(State {
        percentage: display_device.get("org.freedesktop.UPower.Device", "Percentage")?,
        state: UPowerState::from_u64(
            display_device.get::<u32>("org.freedesktop.UPower.Device", "State")? as u64,
        ),
        warning_level: UPowerWarningLevel::from_u64(
            display_device.get::<u32>("org.freedesktop.UPower.Device", "WarningLevel")? as u64,
        ),
        state_changed: true,
        warn_changed: false,
    }));

    let mut rule = dbus::message::MatchRule::new();
    rule.msg_type = Some(dbus::message::MessageType::Signal);
    rule.path = Some("/org/freedesktop/UPower/devices/DisplayDevice".into());
    rule.interface = Some("org.freedesktop.DBus.Properties".into());
    rule.member = Some("PropertiesChanged".into());

    let matcher_state = state.clone();
    conn.add_match(rule, move |_: (), _, msg| {
        let (_, data, _): (
            Option<String>,
            Option<HashMap<String, dbus::arg::Variant<Box<dyn dbus::arg::RefArg>>>>,
            Option<Vec<String>>,
        ) = msg.get3();
        if let Some(data) = data {
            for (key, value) in data.iter() {
                match key.as_str() {
                    "State" => match refarg_as_u64(&value) {
                        Some(v) => {
                            let mut s = matcher_state.borrow_mut();
                            s.state = UPowerState::from_u64(v);
                            s.state_changed = true;
                        }
                        None => (),
                    },
                    "WarningLevel" => match refarg_as_u64(&value) {
                        Some(v) => {
                            let mut s = matcher_state.borrow_mut();
                            s.warning_level = UPowerWarningLevel::from_u64(v);
                            s.warn_changed = true;
                        }
                        None => (),
                    },
                    "Percentage" => match refarg_as_f64(&value) {
                        Some(v) => matcher_state.borrow_mut().percentage = v,
                        None => (),
                    },
                    _ => (),
                }
            }
        }
        true
    })?;

    loop {
        conn.process(Duration::from_millis(10000))?;
        let mut s = state.borrow_mut();
        if s.state_changed {
            match s.state {
                UPowerState::Charging => notify!(
                    notifications,
                    1,
                    "Power status",
                    format!("Battery charging\nCurrent level: {}%", s.percentage).as_str(),
                    1
                ),
                UPowerState::Discharging => notify!(
                    notifications,
                    1,
                    "Power status",
                    format!("Battery discharging\nCurrent level: {}%", s.percentage).as_str(),
                    1
                ),
                UPowerState::NotCharging => notify!(
                    notifications,
                    1,
                    "Power status",
                    format!("Battery not charging\nCurrent level: {}%", s.percentage).as_str(),
                    2
                ),
                UPowerState::Full => notify!(notifications, 1, "Power status", "Battery full", 1),
                UPowerState::Empty => notify!(notifications, 1, "Power status", "Battery empty", 2),
                UPowerState::Unknown => {
                    notify!(notifications, 1, "Power status", "Unknown power state", 2)
                }
            }
            s.state_changed = false;
        }
        if s.warn_changed {
            match s.warning_level {
                UPowerWarningLevel::None => notify!(
                    notifications,
                    2,
                    "Power warning",
                    "Power warning cleared",
                    1
                ),
                UPowerWarningLevel::Discharging => notify!(
                    notifications,
                    2,
                    "Power warning",
                    "Warning: Battery discharging",
                    1
                ),
                UPowerWarningLevel::Low => {
                    notify!(notifications, 2, "Power warning", "Warning: Battery low", 2)
                }
                UPowerWarningLevel::Critical => notify!(
                    notifications,
                    2,
                    "Power warning",
                    "Warning: Battery critical",
                    2
                ),
                UPowerWarningLevel::Action => notify!(
                    notifications,
                    2,
                    "Power warning",
                    "Warning: Battery action",
                    2
                ),
                UPowerWarningLevel::Unknown => notify!(
                    notifications,
                    2,
                    "Power warning",
                    "Warning: Unknown power warning",
                    2
                ),
            }
            s.warn_changed = false;
        }
    }
}

fn main() {
    if let Err(e) = run() {
        eprintln!("error: {}", e);
    }
}
