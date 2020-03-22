project('poweralertd', 'c')

executable(
	'poweralertd',
	['main.c', 'upower.c', 'notify.c'],
	dependencies: [dependency('libsystemd')],
	install: true)