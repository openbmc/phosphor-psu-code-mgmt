oe_sdk = get_option('oe-sdk')
if oe_sdk.enabled()
  # Setup OE SYSROOT
  OECORE_TARGET_SYSROOT = run_command('sh', '-c', 'echo $OECORE_TARGET_SYSROOT').stdout().strip()
  if OECORE_TARGET_SYSROOT == ''
      error('Unable to get $OECORE_TARGET_SYSROOT, check your environment.')
  endif
  message('OE_SYSROOT: ' + OECORE_TARGET_SYSROOT)
  rpath = ':'.join([OECORE_TARGET_SYSROOT + '/lib', OECORE_TARGET_SYSROOT + '/usr/lib'])
  ld_so = run_command('sh', '-c', 'find ' + OECORE_TARGET_SYSROOT + '/lib/ld-*.so | sort -r -n | head -n1').stdout().strip()
  dynamic_linker = '-Wl,-dynamic-linker,' + ld_so
endif

gtest = dependency('gtest', main: true, disabler: true, required: build_tests)
gmock = dependency('gmock', disabler: true, required: build_tests)

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
cdata.set_quoted('PSU_JSON_CONFIG', meson.build_root() + '/psu.json')

configure_file(output: 'config.h',
  configuration: cdata,
)
test_inc = include_directories('.')

utest = executable(
  'utest',
  '../src/utils.cpp',
  'test_utils.cpp',
  include_directories: [psu_inc, test_inc],
  cpp_args: '-flto',
  link_args: oe_sdk.enabled() ? ['-flto', dynamic_linker] : '-flto',
  build_rpath: oe_sdk.enabled() ? rpath : '',
  dependencies: [
    gtest,
    gmock,
    phosphor_logging,
    phosphor_dbus_interfaces,
    sdbusplus,
    ssl,
  ])

test('all', utest)
