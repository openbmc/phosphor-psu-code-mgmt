# phosphor-psu-code-mgmt

phosphor-psu-code-mgmt is a service to provide management for PSU code,
including:

* PSU code version
* PSU code update


## Building

```
meson build/ && ninja -C build
```

## Unit test

* Run it in OpenBMC CI, refer to [local-ci-build.md][1]
* Run it in [OE SDK][2], run below commands in a x86-64 SDK env:
   ```
   meson -Doe-sdk=enabled -Dtests=enabled build/
   ninja -C build/ test  # Meson skips running the case due to it thinks it's cross compiling
   # Manually run the tests
   for t in `find build/test/ -maxdepth 1 -name "test_*"`; do ./$t || break ; done
   ```

## Vendor-specific tools

This repo contains generic code to handle the PSU versions and
updates. It depends on vendor-specific tools to provide the below
functions on the real PSU hardware:
* Get PSU firmware version
* Compare the firmware version
* Update the PSU firmware

It provides configure options for vendor-specific tools for the above functions:
* `PSU_VERSION_UTIL`: It shall be defined as a command-line tool that
accepts the PSU inventory path as input, and outputs the PSU version
string to stdout.
* `PSU_VERSION_COMPARE_UTIL`: It shall be defined as a command-line
tool that accepts one or more PSU version strings, and outputs the
latest version string to stdout.
* `PSU_UPDATE_SERVICE`: It shall be defined as a systemd service that
accepts two arguments:
   * The PSU inventory DBus object;
   * The path of the PSU image(s).

For example:
```
meson -Dtests=disabled \
    '-DPSU_VERSION_UTIL=/usr/bin/psutils --raw --get-version' \
    '-DPSU_VERSION_COMPARE_UTIL=/usr/bin/psutils --raw --compare' \
    '-DPSU_UPDATE_SERVICE=psu-update@.service' \
    build
```

The above configures the vendor-specific tools to use `psutils` from
[phosphor-power][3] to get and compare the PSU versions, and use
`psu-update@.service` to perform the PSU firmware update, where
internally it invokes `psutils` as well.

[1]: https://github.com/openbmc/docs/blob/master/testing/local-ci-build.md
[2]: https://github.com/openbmc/docs/blob/master/cheatsheet.md#building-the-openbmc-sdk
[3]: https://github.com/openbmc/phosphor-power/tree/master/tools/power-utils
