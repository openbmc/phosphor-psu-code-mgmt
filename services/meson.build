unit_files = [
    'xyz.openbmc_project.Software.Psu.Updater.service',
    'psu-update@.service',
]

fs = import('fs')
foreach unit : unit_files
    fs.copyfile(unit, install: true, install_dir: servicedir)
endforeach
