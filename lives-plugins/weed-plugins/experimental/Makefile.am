# Process this with automake to create Makefile.in

LIBS =

fxpluginslibdir = "$(libdir)/$(PACKAGE)/plugins/effects/realtime/weed"

if IS_DARWIN
shared_ldflags = -dynamiclib -module -avoid-version --tag=disable-static
shared_cflags = -fno-common
else
shared_ldflags = -shared -fPIC -DPIC -module -no-undefined -avoid-version --tag=disable-static
shared_cflags = -fPIC -DPIC -Wno-unused-function
endif



if HAVE_SYSTEM_WEED
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@
weed_CFLAGS = -DHAVE_SYSTEM_WEED
else
if IS_MINGW
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@ -D IS_MINGW=1
else
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@
endif
weed_CFLAGS =
endif

if HAVE_SYSTEM_WEED_PLUGIN
weed_CFLAGS = -DHAVE_SYSTEM_WEED_PLUGIN_H
endif

if HAVE_SYSTEM_WEED_COMPAT
weed_CFLAGS += -DHAVE_SYSTEM_WEED_COMPAT
endif

AM_CFLAGS = $(shared_cflags) $(weed_CFLAGS) -ffast-math
AM_CXXFLAGS = $(shared_cflags) $(weed_CFLAGS) -ffast-math


if BUILD_EXPERIMENTAL

AM_CFLAGS += -I${HOME}/src/jdk1.7.0_04/include -I${HOME}/src/jdk1.7.0_04/include/linux

AM_LDFLAGS += -L${HOME}/src/jdk1.7.0_04/jre/lib/amd64/server -ljvm

weka_elmandir = $(fxpluginslibdir)
weka_elman_LTLIBRARIES = weka-elman.la
weka_elman_la_SOURCES = weka-elman.c cj.c cj.h

endif

#	export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${HOME}/src/jdk1.7.0_04/jre/lib/amd64/server



EXTRA_DIST=data icons

install-data-local:
	mkdir -p "$(DESTDIR)$(fxpluginslibdir)"
	cp -rf data/ "$(DESTDIR)$(fxpluginslibdir)" && chmod -Rf a+r "$(DESTDIR)$(fxpluginslibdir)/data/"
	cp -rf icons/ "$(DESTDIR)$(fxpluginslibdir)" && chmod -Rf a+r "$(DESTDIR)$(fxpluginslibdir)/icons/"
	$(MAKE) -i install-data-local-remove-vcs-files

install-data-local-remove-vcs-files:
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/data/".*
	rm -rf "$(DESTDIR)$(fxpluginslibdir)/icons/".*


install-exec-local:



