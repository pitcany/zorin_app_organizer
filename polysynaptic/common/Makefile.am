

# PolySynaptic Common Library
# This library contains package management logic for APT, Snap, and Flatpak

noinst_LIBRARIES = libsynaptic.a

AM_CPPFLAGS = -I/usr/include/apt-pkg @RPM_HDRS@ @DEB_HDRS@ \
	$(LIBEPT_CFLAGS) \
	-DSYNAPTICLOCALEDIR=\""$(synapticlocaledir)"\" \
	-DSYNAPTICSTATEDIR=\""$(localstatedir)"\" \
	-std=c++14

# Original Synaptic sources
libsynaptic_a_SOURCES =\
	pkg_acqfile.cc\
	pkg_acqfile.h\
	rconfiguration.cc\
	rconfiguration.h\
	rinstallprogress.cc\
	rinstallprogress.h\
	rpackage.cc\
	rpackage.h\
	rpackagestatus.cc\
	rpackagestatus.h\
	rpackagecache.cc\
	rpackagecache.h\
	rpackagefilter.cc\
	rpackagefilter.h\
	rpackagelister.cc\
	rpackagelister.h\
	rpackageview.cc\
	rpackageview.h\
	rcdscanner.cc\
	rcdscanner.h\
	rpmindexcopy.cc \
	rpmindexcopy.h \
	raptoptions.cc \
	raptoptions.h\
	rsources.cc \
	rsources.h \
	rcacheactor.cc \
	rcacheactor.h \
	rpackagelistactor.cc \
	rpackagelistactor.h \
	rtagcollbuilder.cc \
	rtagcollbuilder.h \
	i18n.h \
	sections_trans.h \
	rtagcollfilter.cc \
	rtagcollfilter.h \
	ruserdialog.h \
	ruserdialog.cc \
	sections_trans.h \
	sections_trans.cc \
	ipackagebackend.h \
	aptbackend.h \
	aptbackend.cc \
	snapbackend.h \
	snapbackend.cc \
	flatpakbackend.h \
	flatpakbackend.cc \
	backendmanager.h \
	backendmanager.cc \
	packagesourceprovider.h \
	structuredlog.h \
	aptprovider.h \
	aptprovider.cc \
	snapprovider.h \
	snapprovider.cc \
	flatpakprovider.h \
	flatpakprovider.cc \
	packageranking.h \
	packageranking.cc


