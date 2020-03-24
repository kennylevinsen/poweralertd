project('poweralertd', 'c')

executable(
	'poweralertd',
	['main.c', 'upower.c', 'notify.c', 'list.c'],
	dependencies: [dependency('libsystemd')],
	install: true)