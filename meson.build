project('phosphor-psu-code-mgmt', 'cpp',
        version: '0.1', meson_version: '>=0.49.0',
        default_options: [
          'warning_level=3',
          'werror=true',
          'cpp_std=c++17'
        ])

systemd = dependency('systemd')
servicedir = systemd.get_pkgconfig_variable('systemdsystemunitdir')

configure_file(input: 'service/xyz.openbmc_project.Software.Psu.Updater.service',
               output: 'xyz.openbmc_project.Software.Psu.Updater.service',
               copy: true,
               install_dir: servicedir)

conf_dir = get_option('sysconfdir') + '/default/obmc/psu/'

configure_file(input: 'config/example.json',
               output: 'psu.json',
               copy: true,
               install_dir: conf_dir)

# Common configurations for src and test
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
phosphor_logging = dependency('phosphor-logging')
sdbusplus = dependency('sdbusplus')

add_project_link_arguments(['-lstdc++fs'], language: 'cpp')

subdir('src')

build_tests = get_option('tests')

if build_tests.enabled()
  subdir('test')
endif
