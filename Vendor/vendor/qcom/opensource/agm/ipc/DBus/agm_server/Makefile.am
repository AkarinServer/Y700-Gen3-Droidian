pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = agmserver.pc
EXTRA_DIST = $(pkgconfig_DATA)

h_sources = ./inc/agm-dbus-utils.h \
            ./inc/agm_server_wrapper_dbus.h

AM_CPPFLAGS = -I $(srcdir)/inc -I $(top_srcdir)/service/inc/public
AM_CPPFLAGS += -D__unused=__attribute__\(\(__unused__\)\)

lib_LTLIBRARIES = libagmserverwrapper.la
libagmserverwrapper_la_SOURCES = ./src/agm-dbus-utils.cpp ./src/agm_server_wrapper_dbus.cpp
libagmserverwrapper_la_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS) $(GLIB_CFLAGS) -DAGM_USE_SYSLOG
libagmserverwrapper_la_LIBADD = -lagm -L$(top_builddir)/service/.libs
libagmserverwrapper_la_LDFLAGS = -ldl -lpthread -lar_osal -shared -avoid-version $(DBUS_LIBS)

bin_PROGRAMS = agm_server

agm_server_SOURCES = ./src/agm-server-daemon.cpp
agm_server_CPPFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS) -DAGM_USE_SYSLOG -Wno-write-strings
agm_server_LDADD = libagmserverwrapper.la $(GLIB_LIBS)
agm_server_LDFLAGS = -ldl -shared -avoid-version
