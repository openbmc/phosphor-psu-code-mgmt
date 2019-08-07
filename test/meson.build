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

configure_file(output: 'config.h',
  configuration: cdata,
)
test_inc = include_directories('.')

test_util = executable(
  'test_util',
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

test_item_updater = executable(
  'test_item_updater',
  '../src/activation.cpp',
  '../src/item_updater.cpp',
  '../src/version.cpp',
  'test_item_updater.cpp',
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

test('util', test_util)
test('item_updater', test_item_updater)
