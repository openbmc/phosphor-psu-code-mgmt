phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
phosphor_logging = dependency('phosphor-logging')
sdbusplus = dependency('sdbusplus')

executable(
  'phosphor-psu-code-manager',
  'activation.cpp',
  'main.cpp',
  implicit_include_directories: false,
  dependencies: [
    phosphor_logging,
    phosphor_dbus_interfaces,
    sdbusplus,
  ],
  install: true,
  install_dir: get_option('bindir'))
