AM_CPPFLAGS = -I$(top_srcdir)

# gtest unit tests which run during a 'make check'
check_PROGRAMS = utest

# Run all 'check' test programs
TESTS = $(check_PROGRAMS)

# Build/add utest to test suite
utest_CPPFLAGS = \
	-Igtest \
	$(GTEST_CPPFLAGS) \
	$(AM_CPPFLAGS)

utest_CXXFLAGS = \
	$(PTHREAD_CFLAGS) \
	$(PHOSPHOR_LOGGING_CFLAGS) \
	$(PHOSPHOR_DBUS_INTERFACES_CFLAGS)

utest_LDFLAGS = \
	-lgtest_main \
	-lgtest \
	$(PTHREAD_LIBS) \
	$(OESDK_TESTCASE_FLAGS) \
	$(PHOSPHOR_LOGGING_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	-lssl \
	-lcrypto

utest_SOURCES = \
	utest.cpp \
	msl_verify.cpp
utest_LDADD = \
	$(top_builddir)/openpower_update_manager-activation.o \
	$(top_builddir)/openpower_update_manager-version.o \
	$(top_builddir)/openpower_update_manager-serialize.o \
	$(top_builddir)/openpower_update_manager-watch.o \
	$(top_builddir)/openpower_update_manager-item_updater.o \
	$(top_builddir)/org/openbmc/Associations/openpower_update_manager-server.o \
	$(top_builddir)/image_verify.cpp \
	$(top_builddir)/msl_verify.cpp \
	-lstdc++fs
