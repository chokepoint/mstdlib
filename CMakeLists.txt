cmake_minimum_required(VERSION 3.4.3)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/")

include(DefaultBuildType)

project(Mstdlib
	VERSION   1.0.0
	LANGUAGES C
)

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(CheckSymbolExists)
include(CheckTypeSize)
include(SystemExtensions)
include(InstallDepLibs)
include(InstallExports) # Provides install_exports()
include(SetPackageName) # Provides set_package_name()

include(AddressSanitizer) # Provides M_ASAN option, if supported by compiler
include(Harden)
include(EnableWarnings)


# Version info.
set(MSTDLIB_VERSION_MAJOR    ${PROJECT_VERSION_MAJOR})
set(MSTDLIB_VERSION_MINOR    ${PROJECT_VERSION_MINOR})
set(MSTDLIB_VERSION_PATCH    ${PROJECT_VERSION_PATCH})
set(MSTDLIB_VERSION_STRING   ${PROJECT_VERSION})

set(MSTDLIB_SOVERSION_STRING ${MSTDLIB_VERSION_MAJOR})


# Build options
if (M_ASAN)
	set(valgrind_default FALSE)
else ()
	set(valgrind_default TRUE)
endif ()

option(MSTDLIB_STATIC          "Build as a static library"                              FALSE)
option(MSTDLIB_STATIC_PIC      "Build the static library as PIC (position independent)" FALSE)
option(MSTDLIB_INSTALL_HEADERS "Install headers"                                        TRUE)
option(MSTDLIB_INSTALL_LIBS    "Install libraries"                                      TRUE)
option(MSTDLIB_INSTALL_EXPORTS "Install export file"                                    ${MSTDLIB_INSTALL_LIBS})
option(MSTDLIB_BUILD_TESTS     "Build tests"                                            TRUE)
option(MSTDLIB_BUILD_DOCS      "Build doxygen docs"                                     TRUE)
option(MSTDLIB_USE_VALGRIND    "Run tests with valgrind when running 'make check'"      ${valgrind_default})
option(MSTDLIB_BUILD_FORMATS   "Build formats module"                                   TRUE)
option(MSTDLIB_BUILD_THREAD    "Build thread module"                                    TRUE)
option(MSTDLIB_BUILD_IO        "Build io module"                                        TRUE)
option(MSTDLIB_BUILD_TLS       "Build tls module"                                       TRUE)
option(MSTDLIB_BUILD_LOG       "Build log module"                                       TRUE)
option(MSTDLIB_BUILD_SQL       "Build sql module"                                       TRUE)

mark_as_advanced(FORCE MSTDLIB_INSTALL_EXPORTS)

# Keep build organized.
if (NOT MSTDLIB_RUNTIME_OUTPUT_DIRECTORY)
	set(MSTDLIB_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif ()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MSTDLIB_RUNTIME_OUTPUT_DIRECTORY}")

if (NOT MSTDLIB_LIBRARY_OUTPUT_DIRECTORY)
	set(MSTDLIB_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif ()
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${MSTDLIB_LIBRARY_OUTPUT_DIRECTORY}")

if (NOT MSTDLIB_ARCHIVE_OUTPUT_DIRECTORY)
	set(MSTDLIB_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif ()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${MSTDLIB_ARCHIVE_OUTPUT_DIRECTORY}")


# Add target containing include directories for "internal" headers that are used across
# the different modules in mstdlib. (config.h, m_config.h, config.w32.h, base/m_defs_int.h)
#
# Also include any mstdlib-wide compiler defines we'd like to set.
add_library(mstdlib_config INTERFACE)
target_include_directories(mstdlib_config INTERFACE
	"${CMAKE_CURRENT_BINARY_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/base"
)
if (WIN32)
	target_compile_definitions(mstdlib_config INTERFACE
		# Suppress annoying deprecation warnings on newer windows.
		_WINSOCK_DEPRECATED_NO_WARNINGS
		_CRT_SECURE_NO_DEPRECATE
		_CRT_NONSTDC_NO_DEPRECATE
		# Leave out little-used parts of windows.h (can still get them by including the specific headers you need).
		WIN32_LEAN_AND_MEAN
		NOMINMAX
		# Explicitly request Windows Vista or newer feature level.
		_WIN32_WINNT=0x0600
	)
endif ()
# Add alias for this target, for consumption by the different modules.
add_library(Mstdlib::config ALIAS mstdlib_config)


# Used by subdirectories to build paths to other subdirectories.
set(MSTDLIB_TOPLEVEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Platform checks
if (APPLE)
	CHECK_C_SOURCE_COMPILES ("
	#include <stdio.h>
	#include <TargetConditionals.h>
	int main() {
#if TARGET_OS_IPHONE == 0
#error Not an iPhone target
#endif
return 0;
	}
	"
	IOS)

	CHECK_C_SOURCE_COMPILES ("
	#include <stdio.h>
	#include <TargetConditionals.h>
	int main() {
#if TARGET_IPHONE_SIMULATOR == 0
#error Not an iPhone simulator target
#endif
return 0;
	}
	"
	IOSSIM)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Android")
	set(ANDROID TRUE)
endif ()

# Check Types
# CHECK_TYPE_SIZE can't be used to see if a type exists because on Apple when
# building multi-arch, it will throw an error.  So we need to wrap
# CHECK_C_SOURCE_COMPILES for our tests.
function(mstdlib_type_exists type includes var)
	set(_MSTDLIB_C_SOURCE "
		#include <stdio.h>
		#include <stdlib.h>
	")
	foreach (_C_HEADER ${includes})
		set(_MSTDLIB_C_SOURCE "${_MSTDLIB_C_SOURCE}
		#include <${_C_HEADER}>")
	endforeach ()

	set(_MSTDLIB_C_SOURCE "${_MSTDLIB_C_SOURCE}
		int main() {
			${type} var_exists;
			return 0;
		}
	")
	check_c_source_compiles("${_MSTDLIB_C_SOURCE}" ${var})
endfunction()

# Helper function that adds variable to list if a given flag variable is true.
# Args:
#    _listvar:      name of list variable to append to
#    _flagvar:      name of flag variable to check
#    ... (var arg): stuff to append
function(list_append_if_set _listvar _flagvar)
	if (${_flagvar})
		list(APPEND ${_listvar} "${ARGN}")
		set(${_listvar} "${${_listvar}}" PARENT_SCOPE)
	endif ()
endfunction()

# Check for required system functionality.
check_include_files(arpa/inet.h         HAVE_ARPA_INET_H)
check_include_files(errno.h             HAVE_ERRNO_H)
check_include_files(io.h                HAVE_IO_H)
check_include_files(netdb.h             HAVE_NETDB_H)
check_include_files(netinet/in.h        HAVE_NETINET_IN_H)
check_include_files(netinet/tcp.h       HAVE_NETINET_TCP_H)
check_include_files(signal.h            HAVE_SIGNAL_H)
check_include_files(stdalign.h          HAVE_STDALIGN_H)
check_include_files(stddef.h            HAVE_STDDEF_H)
check_include_files(stdlib.h            HAVE_STDLIB_H)
check_include_files(string.h            HAVE_STRING_H)
check_include_files(strings.h           HAVE_STRINGS_H)
check_include_files(sys/ioctl.h         HAVE_SYS_IOCTL_H)
check_include_files(sys/select.h        HAVE_SYS_SELECT_H)
check_include_files(sys/socket.h        HAVE_SYS_SOCKET_H)
check_include_files(sys/time.h          HAVE_SYS_TIME_H)
check_include_files(sys/types.h         HAVE_SYS_TYPES_H)
check_include_files(sys/uio.h           HAVE_SYS_UIO_H)
check_include_files(sys/un.h            HAVE_SYS_UN_H)
check_include_files(time.h              HAVE_TIME_H)
check_include_files(ucontext.h          HAVE_UCONTEXT_H)
check_include_files(unistd.h            HAVE_UNISTD_H)
check_include_files(valgrind/valgrind.h HAVE_VALGRIND_H)
# Include order matters for these windows files.
check_include_files("winsock2.h;windows.h"            HAVE_WINSOCK2_H)
check_include_files("winsock2.h;ws2tcpip.h;windows.h" HAVE_WS2TCPIP_H)
check_include_files("winsock.h;windows.h"             HAVE_WINSOCK_H)
check_include_files(windows.h                         HAVE_WINDOWS_H)

# Setup for check_symbol_exists.
set(check_extra_includes)
list_append_if_set(check_extra_includes HAVE_ARPA_INET_H arpa/inet.h)
list_append_if_set(check_extra_includes HAVE_SIGNAL_H    signal.h)
list_append_if_set(check_extra_includes HAVE_STDLIB_H    stdlib.h)
list_append_if_set(check_extra_includes HAVE_STRING_H    string.h)
list_append_if_set(check_extra_includes HAVE_STRINGS_H   strings.h)
list_append_if_set(check_extra_includes HAVE_TIME_H      time.h)
list_append_if_set(check_extra_includes HAVE_UCONTEXT_H  ucontext.h)
list_append_if_set(check_extra_includes HAVE_UNISTD_H    unistd.h)
list_append_if_set(check_extra_includes HAVE_WS2TCPIP_H  winsock2.h ws2tcpip.h windows.h)

set(CMAKE_REQUIRED_DEFINITIONS)
list_append_if_set(CMAKE_REQUIRED_DEFINITIONS _GNU_SOURCE     -D_GNU_SOURCE)
list_append_if_set(CMAKE_REQUIRED_DEFINITIONS _MINIX          -D_MINIX)
list_append_if_set(CMAKE_REQUIRED_DEFINITIONS _POSIX_1_SOURCE -D_POSIX_1_SOURCE)
list_append_if_set(CMAKE_REQUIRED_DEFINITIONS _POSIX_SOURCE   -D_POSIX_SOURCE)

# Check for presence of various functions in include files and libs.
check_symbol_exists(strcasecmp    "${check_extra_includes}" HAVE_STRCASECMP)
check_symbol_exists(strncasecmp   "${check_extra_includes}" HAVE_STRNCASECMP)
check_symbol_exists(_stricmp      "${check_extra_includes}" HAVE__STRICMP)
check_symbol_exists(_strnicmp     "${check_extra_includes}" HAVE__STRNICMP)
check_symbol_exists(localtime_r   "${check_extra_includes}" HAVE_LOCALTIME_R)
check_symbol_exists(inet_pton     "${check_extra_includes}" HAVE_INET_PTON)
check_symbol_exists(inet_ntop     "${check_extra_includes}" HAVE_INET_NTOP)
check_symbol_exists(getcontext    "${check_extra_includes}" HAVE_GETCONTEXT)
check_symbol_exists(secure_getenv "${check_extra_includes}" HAVE_SECURE_GETENV)
check_symbol_exists(sigtimedwait  "${check_extra_includes}" HAVE_SIGTIMEDWAIT)
check_symbol_exists(sigwait       "${check_extra_includes}" HAVE_SIGWAIT)

check_struct_has_member("struct dirent" d_type    dirent.h HAVE_DIRENT_TYPE)
check_struct_has_member("struct tm"     tm_gmtoff time.h   STRUCT_TM_HAS_GMTOFF)
check_struct_has_member("struct tm"     tm_zone   time.h   STRUCT_TM_HAS_ZONE)

check_symbol_exists("alignof" stdalign.h HAVE_ALIGNOF)
include(MaxAlignt)

check_library_exists(rt clock_gettime "" NEED_RT)

include(CheckHasGetPWUID)
include(CheckHasGetPWNAM)
include(CheckHasGetGRGID)
include(CheckHasGetGRNAM)
include(CheckHasBirthTime)
include(CheckHasVaCopy)
include(ReadDir)

if (NOT ANDROID)
	include(LargeFiles)
endif ()

if (MSTDLIB_BUILD_THREAD)
	# Need to prefer pthreads on platforms that may have more threading choices (e.g. Solaris)
	set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
	find_package(Threads)
	if (CMAKE_HAVE_PTHREAD_H AND NOT MINGW)
		set(HAVE_PTHREAD TRUE)

		# Fix solaris9 bug due to libc having pthread_create() stubs that always fail.  CMake
		# doesn't realize that the real pthread functions aren't in libc, so sets the pthread
		# library CAKE_THREAD_LIBS_INIT variable to blank instead of to the correct "-lpthread".
		if (CMAKE_SYSTEM_NAME STREQUAL "SunOS" AND NOT CMAKE_THREAD_LIBS_INIT)
			set(CMAKE_THREAD_LIBS_INIT "-lpthread")
		endif ()

		# PThread functions.
		check_include_files(pthread.h                                 HAVE_PTHREAD_H)
		check_library_exists(pthread pthread_yield                 "" HAVE_PTHREAD_YIELD)
		check_library_exists(pthread pthread_rwlock_init           "" HAVE_PTHREAD_RWLOCK_INIT)
		check_library_exists(pthread pthread_rwlockattr_setkind_np "" HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP)

		if (HAVE_PTHREAD_H)
			check_symbol_exists(pthread_init "pthread.h" HAVE_PTHREAD_INIT)
		endif ()

		# Used in config.h.cmake.
		check_symbol_exists(sysconf   "${check_extra_includes}" HAVE_SYSCONF)

		# Determine sleep method.
		check_include_files(poll.h              HAVE_POLL_H)
		check_include_files(sys/poll.h          HAVE_SYS_POLL_H)
		list_append_if_set(check_extra_includes HAVE_POLL_H     poll.h)
		list_append_if_set(check_extra_includes HAVE_SYS_POLL_H sys/poll.h)
		check_symbol_exists(poll      "${check_extra_includes}" HAVE_POLL)
		check_symbol_exists(select    "${check_extra_includes}" HAVE_SELECT)
		check_symbol_exists(nanosleep "${check_extra_includes}" HAVE_NANOSLEEP)

		if ((CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND NOT IOS AND NOT IOSSIM)
			OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD"
			OR CMAKE_SYSTEM_NAME STREQUAL "HP-UX")
			set(PTHREAD_SLEEP_USE_SELECT TRUE)
		elseif (HAVE_POLL)
			set(PTHREAD_SLEEP_USE_POLL TRUE)
		elseif (HAVE_SELECT)
			set(PTHREAD_SLEEP_USE_SELECT TRUE)
		elseif (HAVE_NANOSLEEP)
			set(PTHREAD_SLEEP_USE_NANOSLEEP TRUE)
		else ()
			message(FATAL_ERROR "Valid sleep implementation not avalible")
		endif ()
	endif ()
	check_include_files(atomic.h HAVE_ATOMIC_H)
endif ()


if (MSTDLIB_BUILD_IO)
	check_include_files(arpa/nameser_compat.h HAVE_ARPA_NAMESER_COMPAT_H)
	check_include_files(arpa/nameser.h        HAVE_ARPA_NAMESER_H)
	check_include_files(fcntl.h               HAVE_FCNTL_H)
	check_include_files(net/if.h              HAVE_NET_IF_H)
	check_include_files(socket.h              HAVE_SOCKET_H)
	check_include_files(sys/epoll.h           HAVE_SYS_EPOLL_H)
	check_include_files(sys/event.h           HAVE_SYS_EVENT_H)

	# Setup for check_symbol_exists.
	list_append_if_set(check_extra_includes HAVE_ARPA_NAMESER_H arpa/nameser.h)
	list_append_if_set(check_extra_includes HAVE_FCNTL_H        fcntl.h)
	list_append_if_set(check_extra_includes HAVE_NETDB_H        netdb.h)
	list_append_if_set(check_extra_includes HAVE_NETINET_IN_H   netinet/in.h)
	list_append_if_set(check_extra_includes HAVE_NETINET_TCP_H  netinet/tcp.h)
	list_append_if_set(check_extra_includes HAVE_SYS_EPOLL_H    sys/epoll.h)
	list_append_if_set(check_extra_includes HAVE_SYS_EVENT_H    sys/event.h)
	list_append_if_set(check_extra_includes HAVE_SYS_IOCTL_H    sys/ioctl.h)
	list_append_if_set(check_extra_includes HAVE_SYS_SELECT_H   sys/select.h)
	list_append_if_set(check_extra_includes HAVE_SYS_SOCKET_H   sys/socket.h)
	list_append_if_set(check_extra_includes HAVE_SYS_TIME_H     sys/time.h)
	list_append_if_set(check_extra_includes HAVE_SYS_TYPES_H    sys/types.h)
	list_append_if_set(check_extra_includes HAVE_SYS_UIO_H      sys/uio.h)

	# Checks for presence of various functions and types.
	check_symbol_exists(accept4       "${check_extra_includes}" HAVE_ACCEPT4)
	check_symbol_exists(epoll_create  "${check_extra_includes}" HAVE_EPOLL)
	check_symbol_exists(epoll_create1 "${check_extra_includes}" HAVE_EPOLL_CREATE1)
	check_symbol_exists(kqueue        "${check_extra_includes}" HAVE_KQUEUE)
	check_symbol_exists(pipe2         "${check_extra_includes}" HAVE_PIPE2)

	mstdlib_type_exists(socklen_t                 "${check_extra_includes}" HAVE_SOCKLEN_T)
	mstdlib_type_exists("struct sockaddr_storage" "${check_extra_includes}" HAVE_SOCKADDR_STORAGE)
endif ()

if (MSTDLIB_BUILD_SQL)
	check_include_files(dlfcn.h HAVE_DLFCN_H)

	# Setup for check_symbol_exists.
	list_append_if_set(check_extra_includes HAVE_DLFCN_H dlfcn.h)

	# Checks for presence of various functions and types.
	check_library_exists(dl dlopen   ""                        NEED_DL)
	check_symbol_exists(dlopen       "${check_extra_includes}" HAVE_DLOPEN)
	check_symbol_exists(LoadLibrary  "${check_extra_includes}" HAVE_LOADLIBRARY)
endif()

configure_file(config.h.cmake "${PROJECT_BINARY_DIR}/config.h")


# Docs
if (MSTDLIB_BUILD_DOCS)
	find_package(Doxygen)
	if (DOXYGEN_FOUND)
		configure_file(Doxyfile.cmake "${PROJECT_BINARY_DIR}/Doxyfile" @ONLY)
		add_custom_target(mstdlib-doc ALL
			COMMAND "${DOXYGEN_EXECUTABLE}" "${PROJECT_BINARY_DIR}/Doxyfile"
			SOURCES "${PROJECT_BINARY_DIR}/Doxyfile"
			COMMENT "Generating API documentation with Doxygen"
			VERBATIM
		)
		install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs/html" DESTINATION share/${PROJECT_NAME}/doc)
		# Doxygen man pages seem to generate links for things like structure members.  We need to
		# exclude these non-namespaced items, so we're just going to install files that begin with
		# m_ or M_
		install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs/man" DESTINATION share
			FILES_MATCHING PATTERN "M_*" PATTERN "m_*"
		)
	endif ()
endif ()


# Installation location
if (NOT MSTDLIB_INSTALL_LOCATION_LIBS)
	set(MSTDLIB_INSTALL_LOCATION_LIBS    lib)
endif ()
if (NOT MSTDLIB_INSTALL_LOCATION_HEADERS)
	set(MSTDLIB_INSTALL_LOCATION_HEADERS include)
endif ()
if (NOT MSTDLIB_INSTALL_LOCATION_SHARE)
	set(MSTDLIB_INSTALL_LOCATION_SHARE   share)
endif ()

set(lib_dest
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION ${MSTDLIB_INSTALL_LOCATION_LIBS}
	ARCHIVE DESTINATION ${MSTDLIB_INSTALL_LOCATION_LIBS}
)

if (WIN32)
	# On windows, we want to place module DLL's in the same directory as the library DLL's.
	set(mod_dest
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION bin
		# Intentionally leave out archive dest, don't want to install static versions of plugins.
	)
else ()
	set(mod_dest
		LIBRARY DESTINATION ${MSTDLIB_INSTALL_LOCATION_LIBS}
		# Intentionally leave out archive dest, don't want to install static versions of plugins.
	)
endif ()


# Subdirs.
add_subdirectory(include)

add_subdirectory(base)
message(STATUS "Building MSTDLIB base... Enabled")

# Selective component building. If not being built setting the not build flag
# because something could be disabled due to missing dependencies and we don't
# want headers being installed unless it's going to be built.
if (MSTDLIB_BUILD_FORMATS)
	message(STATUS "Building MSTDLIB formats ... Enabled")
	add_subdirectory(formats)
else ()
	message(STATUS "Building MSTDLIB formats ... Disabled")
endif ()

if (MSTDLIB_BUILD_THREAD)
	message(STATUS "Building MSTDLIB thread... Enabled")
	add_subdirectory(thread)
else ()
	message(STATUS "Building MSTDLIB thread... Disabled")
endif ()

if (MSTDLIB_BUILD_IO AND MSTDLIB_BUILD_THREAD)
	message(STATUS "Building MSTDLIB io... Enabled")
	add_subdirectory(io)
else ()
	message(STATUS "Building MSTDLIB io... Disabled")
	set(MSTDLIB_BUILD_IO FALSE CACHE BOOL "" FORCE)
endif ()

if (MSTDLIB_BUILD_TLS AND MSTDLIB_BUILD_THREAD AND MSTDLIB_BUILD_IO)
	message(STATUS "Building MSTDLIB tls... Enabled")
	add_subdirectory(tls)
else ()
	message(STATUS "Building MSTDLIB tls... Disabled")
	set(MSTDLIB_BUILD_TLS FALSE CACHE BOOL "" FORCE)
endif ()

if (MSTDLIB_BUILD_LOG AND MSTDLIB_BUILD_THREAD AND MSTDLIB_BUILD_IO)
	message(STATUS "Building MSTDLIB log... Enabled")
	add_subdirectory(log)
else ()
	message(STATUS "Building MSTDLIB log... Disabled")
	set(MSTDLIB_BUILD_LOG FALSE CACHE BOOL "" FORCE)
endif ()

if (MSTDLIB_BUILD_SQL AND MSTDLIB_BUILD_THREAD)
	message(STATUS "Building MSTDLIB sql... Enabled")
	add_subdirectory(sql)
else ()
	message(STATUS "Building MSTDLIB sql... Disabled")
	set(MSTDLIB_BUILD_SQL FALSE CACHE BOOL "" FORCE)
endif ()

# Tests need to check for the test frame work so it will handle enabled/disable itself.
add_subdirectory(test)

# Trick to make headers and scripts show up in IDEs. Adds a phony target that references the files.
file(GLOB_RECURSE headers_and_scripts *.h *.lua *.yml)
add_custom_target(_mstdlib_root_extra_files ALL SOURCES ${headers_and_scripts})

# If mstdlib is the root project, make sure to include required system libs (if any).
if (MSTDLIB_INSTALL_LIBS AND (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR))
	# Install any required system libs, if any (usually just MSVC redistributables on windows, or winpthreads if building with MinGW).
	install_system_deplibs(${MSTDLIB_INSTALL_LOCATION_LIBS} bin)
endif ()

if (MSTDLIB_INSTALL_EXPORTS)
	install_exports(Mstdlib "${MSTDLIB_INSTALL_LOCATION_SHARE}/Mstdlib/cmake")
endif ()

# If we're not being chain-built, and install is enabled, add code to allow package creation.
if (MSTDLIB_INSTALL_LIBS AND (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR))
	set(CPACK_GENERATOR ZIP)

	if (WIN32)
		# Check to see if Wix is available. If it is, enable wix packaging by default.
		if (NOT CYGWIN AND (EXISTS "$ENV{WIX}" OR EXISTS "${CPACK_WIX_ROOT}"))
			set(wix_default TRUE)
		else ()
			set(wix_default FALSE)
		endif ()

		option(M_PACKAGE_WIX "Use Wix Toolset to generate MSI installer" ${wix_default})
		if (M_PACKAGE_WIX)
			list(APPEND CPACK_GENERATOR WIX)

			# Uniterm builds that share the same GUID are allowed to replace one another on an install (i.e., upgrade in place).
			set(CPACK_WIX_UPGRADE_GUID 174F7115-8000-40D7-A78F-07DF0D2022B3)

			# These variables determine what info shows up for mstdlib in add/remove programs.
			set(CPACK_WIX_PROPERTY_ARPHELPLINK     https://github.com/Monetra/mstdlib)
			set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT https://monetra.github.io/mstdlib)

			# Add installed Mstdlib to the package registry.
			set(CPACK_WIX_CMAKE_PACKAGE_REGISTRY Mstdlib)
		endif ()
	elseif (APPLE)
		# TODO: implement app bundling, the stuff commented out below isn't done yet
	else () # Linux, FreeBSD, etc.
		# Check to see if rpm is available.
		find_program(RPMBUILD_EXECUTABLE rpmbuild)
		mark_as_advanced(FORCE RPMBUILD_EXECUTABLE)
		if (RPMBUILD_EXECUTABLE)
			option(M_PACKAGE_RPM "Add RPM to packaging output" TRUE)
		else ()
			set(M_PACKAGE_RPM FALSE)
		endif ()
		if (M_PACKAGE_RPM)
			list(APPEND CPACK_GENERATOR RPM)
			set(CPACK_RPM_PACKAGE_URL         https://github.com/monetra/mstdlib)
			set(CPACK_RPM_PACKAGE_RELOCATABLE TRUE)
			# Note: Set CPACK_RPM_main_POST_INSTALL_SCRIPT_FILE to a script file to run post-install (if needed)
		endif ()
	endif ()

	if (CPACK_GENERATOR)
		message(STATUS "Packagers enabled: ${CPACK_GENERATOR}")
	endif ()
	set(CPACK_VERBATIM_VARIABLES TRUE)

	set(CPACK_PACKAGE_VENDOR                "Main Street Softworks")
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY   "M Standard Library (mstdlib)")
	set(CPACK_RESOURCE_FILE_README  "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
	if (WIN32)
		set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_BINARY_DIR}/license.txt")
		file(COPY "LICENSE" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
		file(RENAME "${CMAKE_CURRENT_BINARY_DIR}/LICENSE" "${CPACK_RESOURCE_FILE_LICENSE}")
	else ()
		set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
	endif ()

	set_package_name(${PROJECT_NAME} ${PROJECT_VERSION})

	set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
	set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
	set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
	include(CPack)
endif ()
