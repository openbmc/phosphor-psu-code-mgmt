phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
phosphor_logging = dependency('phosphor-logging')
sdbusplus = dependency('sdbusplus')

add_project_link_arguments(['-lstdc++fs'], language: 'cpp')

cdata = configuration_data()
cdata.set_quoted('SOFTWARE_OBJPATH', '/xyz/openbmc_project/software')
cdata.set_quoted('VERSION_IFACE', 'xyz.openbmc_project.Software.Version')
cdata.set_quoted('FILEPATH_IFACE', 'xyz.openbmc_project.Common.FilePath')
cdata.set_quoted('BUSNAME_UPDATER', 'xyz.openbmc_project.Software.Psu.Updater')
cdata.set_quoted('MANIFEST_FILE', 'MANIFEST')

configure_file(output: 'config.h',
  configuration: cdata,
)
configuration_inc = include_directories('.')

executable(
  'phosphor-psu-code-manager',
  'activation.cpp',
  'item_updater.cpp',
  'main.cpp',
  'version.cpp',
  include_directories: configuration_inc,
  dependencies: [
    phosphor_logging,
    phosphor_dbus_interfaces,
    sdbusplus,
  ],
  install: true,
  install_dir: get_option('bindir'))
