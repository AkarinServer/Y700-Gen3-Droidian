pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = agm.pc
EXTRA_DIST = $(pkgconfig_DATA)

h_sources = ./inc/public/agm/agm_api.h \
            ./inc/public/agm/agm_list.h \
            ./inc/public/agm/utils.h

AM_CFLAGS = @SPF_CFLAGS@
AM_CFLAGS += -I $(srcdir)/inc/public
AM_CFLAGS += -I $(srcdir)/inc/private
AM_CFLAGS += @MMHEADERS_CFLAGS@
library_include_HEADERS = $(h_sources)
library_includedir = $(includedir)/agm

agm_sources = ./src/graph.c \
              ./src/graph_module.c \
              ./src/device.c \
              ./src/device_hw_ep.c \
              ./src/metadata.c \
              ./src/session_obj.c \
              ./src/utils.c \
              ./src/agm.c

lib_LTLIBRARIES = libagm.la
libagm_la_SOURCES = $(agm_sources)
libagm_la_LIBADD = -ltinyalsa -lar_osal -lgsl -lats

libagm_la_CFLAGS = $(AM_CFLAGS) -DACDB_PATH=\"/etc/acdbdata/MTP/\" -DACDB_DELTA_FILE_PATH="/data/audio/delta"
libagm_la_CFLAGS += -DCARD_STATE_UNSUPPORTED

if USE_SYSLOG
libagm_la_CFLAGS += -DAGM_USE_SYSLOG
endif
libagm_la_CFLAGS += -DAGM_MEMLOG_UNSUPPORTED

if USE_GLIB
libagm_la_LIBADD += -lglib-2.0
libagm_la_CFLAGS += $(GLIB_CFLAGS) -Dstrlcpy=g_strlcpy -Dstrlcat=g_strlcat -include glib.h
endif

libagm_la_CFLAGS += -D__unused=__attribute__\(\(__unused__\)\)
libagm_la_LDFLAGS = -module -shared -avoid-version
