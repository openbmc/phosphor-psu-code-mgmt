AM_DEFAULT_SOURCE_EXT = .cpp

ACLOCAL_AMFLAGS = -Im4

bin_PROGRAMS = phosphor-psu-code-manager

noinst_LTLIBRARIES = libpsu-code-manager.la

libpsu_code_manager_la_SOURCES = \
	activation.cpp

phosphor_psu_code_manager_SOURCES = main.cpp

phosphor_psu_code_manager_LDADD = libpsu-code-manager.la

if HAVE_SYSTEMD
systemdsystemunit_DATA = \
	xyz.openbmc_project.Software.Psu.Updater.service
endif

generic_cxx_flags = \
	$(PHOSPHOR_DBUS_INTERFACES_CFLAGS) \
	$(SDBUSPLUS_CFLAGS) \
	$(PHOSPHOR_LOGGING_CFLAGS) \
	-flto

generic_ld_flags = \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	$(SDBUSPLUS_LIBS) \
	$(PHOSPHOR_LOGGING_LIBS)

libpsu_code_manager_la_CXXFLAGS = $(generic_cxx_flags)
libpsu_code_manager_la_LIBADD = $(generic_ld_flags)

phosphor_psu_code_manager_CXXFLAGS = $(generic_cxx_flags)

phosphor_psu_code_manager_LDFLAGS = $(generic_ld_flags)

