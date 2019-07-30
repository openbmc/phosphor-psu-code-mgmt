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

subdir('src')
