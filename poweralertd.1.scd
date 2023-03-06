poweralertd(1)

# NAME

poweralertd - power alert notifications

# SYNOPSIS

*poweralertd*

# OPTIONS

*-h*
	Show help message and quit.

*-s*
	Ignore events within 1 second of startup.

*-i <device_type>*
	Ignore this device type. Can be used several times.

# DESCRIPTION

poweralertd monitors all power devices known to UPower, and provides
information about their state and warning levels when these changes. Most
importantly, this allows the user to mitigate power issues before they become
critical.

# AUTHORS

Maintained by Kenny Levinsen <contact@kl.wtf>. For more information about
poweralertd development, see https://git.sr.ht/~kennylevinsen/poweralertd.
