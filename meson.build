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

logind = dependency('libsystemd', required: false)
if logind.found()
	add_project_arguments('-DHAVE_SYSTEMD=1', language: 'c')
else
	logind = dependency('libelogind')
	add_project_arguments('-DHAVE_ELOGIND=1', language: 'c')
endif

executable(
	'poweralertd',
	['main.c', 'upower.c', 'notify.c', 'list.c'],
	dependencies: [logind],
	install: true)