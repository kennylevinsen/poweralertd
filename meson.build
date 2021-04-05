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

add_project_arguments('-Wno-unused-parameter', language: 'c')
add_project_arguments('-Wno-missing-braces', language: 'c')

configuration = configuration_data()
configuration.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))

basu = dependency('basu', required: false)
elogind = dependency('libelogind', required: false)
systemd = dependency('libsystemd', required: false)

if basu.found()
	add_project_arguments('-DHAVE_BASU=1', language: 'c')
	sdbus = basu
elif elogind.found()
	add_project_arguments('-DHAVE_ELOGIND=1', language: 'c')
	sdbus = elogind
elif systemd.found()
	add_project_arguments('-DHAVE_SYSTEMD=1', language: 'c')
	sdbus = systemd

	systemd = dependency('systemd', required: true)
	systemd_units_dir = systemd.get_pkgconfig_variable('systemduserunitdir')
	configure_file(
		configuration: configuration,
		input: 'contrib/systemd-user/poweralertd.service.in',
		output: '@BASENAME@',
		install_dir: systemd_units_dir
	)
else
	dependency('basu')
endif

executable(
	'poweralertd',
	['main.c', 'upower.c', 'notify.c', 'list.c'],
	dependencies: [sdbus],
	install: true,
)

scdoc = dependency('scdoc', required: get_option('man-pages'), version: '>= 1.9.7', native: true)

if scdoc.found()
	scdoc_prog = find_program(scdoc.get_pkgconfig_variable('scdoc'), native: true)
	sh = find_program('sh', native: true)

	man_pages = ['poweralertd.1.scd']

	mandir = get_option('mandir')

	foreach src : man_pages
		topic = src.split('.')[0]
		section = src.split('.')[1]
		output = '@0@.@1@'.format(topic, section)

		custom_target(
			output,
			input: src,
			output: output,
			command: [
				sh, '-c', '@0@ < @INPUT@ > @1@'.format(scdoc_prog.path(), output)
			],
			install: true,
			install_dir: '@0@/man@1@'.format(mandir, section)
		)
	endforeach
endif
