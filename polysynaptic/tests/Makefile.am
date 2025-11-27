# PolySynaptic Test Suite

AM_CPPFLAGS = -I${top_srcdir}/common -I${top_srcdir}/gtk \
	@GTK_CFLAGS@ @VTE_CFLAGS@ @LP_CFLAGS@ $(LIBTAGCOLL_CFLAGS) $(LIBEPT_CFLAGS) \
	-O0 -g3 -std=c++14

noinst_PROGRAMS = test_rpackage test_rpackageview test_gtkpkglist test_rpackagefilter test_backends test_providers

LDADD = \
	${top_builddir}/common/libsynaptic.a\
	-lapt-pkg -lX11 @RPM_LIBS@ @DEB_LIBS@ \
	@GTK_LIBS@ @VTE_LIBS@ @LP_LIBS@ @XAPIAN_LIBS@ \
	-lpthread

# Original Synaptic tests
test_rpackage_SOURCES= test_rpackage.cc

test_rpackagefilter_SOURCES= test_rpackagefilter.cc

test_rpackageview_SOURCES= test_rpackageview.cc

test_gtkpkglist_SOURCES= test_gtkpkglist.cc \
	${top_srcdir}/gtk/rgpackagestatus.cc\
	${top_srcdir}/gtk/rgutils.cc\
	${top_srcdir}/gtk/rgpkgtreeview.cc\
	${top_srcdir}/gtk/gtkpkglist.cc

# PolySynaptic backend tests
test_backends_SOURCES= test_backends.cc

# PolySynaptic provider integration tests
test_providers_SOURCES= test_providers.cc

# Run all tests
check-local: test_backends test_providers
	./test_backends
	./test_providers

CLEANFILES= $(wildcard *_wrap.*) $(wildcard *~) 

