cdata = configuration_data()
cdata.set_quoted('VERSION_IFACE', 'xyz.openbmc_project.Software.Version')
cdata.set_quoted('FILEPATH_IFACE', 'xyz.openbmc_project.Common.FilePath')
cdata.set_quoted('BUSNAME_UPDATER', 'xyz.openbmc_project.Software.Psu.Updater')
cdata.set_quoted('ACTIVATION_FWD_ASSOCIATION', 'inventory')
cdata.set_quoted('ACTIVATION_REV_ASSOCIATION', 'activation')
cdata.set_quoted('ACTIVE_FWD_ASSOCIATION', 'active')
cdata.set_quoted('ACTIVE_REV_ASSOCIATION', 'software_version')
cdata.set_quoted('FUNCTIONAL_FWD_ASSOCIATION', 'functional')
cdata.set_quoted('FUNCTIONAL_REV_ASSOCIATION', 'software_version')

cdata.set_quoted('SOFTWARE_OBJPATH', get_option('SOFTWARE_OBJPATH'))
cdata.set_quoted('MANIFEST_FILE', get_option('MANIFEST_FILE'))
cdata.set_quoted('PSU_INVENTORY_PATH', get_option('PSU_INVENTORY_PATH'))
cdata.set_quoted('PSU_JSON_CONFIG', get_option('PSU_JSON_CONFIG'))

configure_file(output: 'config.h',
  configuration: cdata,
)
psu_inc = include_directories('.')

executable(
  'phosphor-psu-code-manager',
  'activation.cpp',
  'item_updater.cpp',
  'main.cpp',
  'version.cpp',
  'utils.cpp',
  include_directories: psu_inc,
  cpp_args: '-flto',
  link_args: '-flto',
  dependencies: [
    phosphor_logging,
    phosphor_dbus_interfaces,
    sdbusplus,
  ],
  install: true,
  install_dir: get_option('bindir'))
