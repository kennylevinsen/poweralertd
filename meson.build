project(
	'poweralertd',
	'c',
	license: 'GPL3',
	meson_version: '>=0.47.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

executable(
	'poweralertd',
	['main.c', 'upower.c', 'notify.c', 'list.c'],
	dependencies: [dependency('libsystemd')],
	install: true)