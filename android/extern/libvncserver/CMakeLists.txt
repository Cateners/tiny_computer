cmake_minimum_required(VERSION 3.4)

project(LibVNCServer VERSION 0.9.14 LANGUAGES C)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckIncludeFile)
include(CheckTypeSize)
include(TestBigEndian)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(GNUInstallDirs)

enable_testing()

set(PACKAGE_NAME           "LibVNCServer")
set(FULL_PACKAGE_NAME      "LibVNCServer")
set(VERSION_SO             "1")
set(PROJECT_BUGREPORT_PATH "https://github.com/LibVNC/libvncserver/issues")
set(LIBVNCSERVER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/libvncserver)
set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/common)
set(LIBVNCCLIENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/libvncclient)
set(LIBVNCSRVEXAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples/server)
set(LIBVNCCLIEXAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples/client)
set(TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_GENERATOR MATCHES "Unix Makefiles|Ninja")
# some LSP servers expect compile_commands.json in the project root
add_custom_target(
    copy-compile-commands ALL
    ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif(CMAKE_GENERATOR MATCHES "Unix Makefiles|Ninja")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${LIBVNCSERVER_DIR}
  ${LIBVNCCLIENT_DIR}
  ${COMMON_DIR})

# all the build configuration switches
option(LIBVNCSERVER_INSTALL "Generate installation target" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ${UNIX})
option(WITH_ZLIB "Search for the zlib compression library to support additional encodings" ON)
option(WITH_LZO "Search for the LZO compression library to omit internal miniLZO implementation" ON)
option(WITH_JPEG "Search for the libjpeg compression library to support additional encodings" ON)
option(WITH_PNG "Search for the PNG compression library to support additional encodings" ON)
option(WITH_SDL "Search for the Simple Direct Media Layer library to build an example SDL vnc client" ON)
option(WITH_GTK "Search for the GTK library to build an example GTK vnc client" ON)
option(WITH_LIBSSH2 "Search for libssh2 to build an example ssh-tunneled client" ON)
option(WITH_THREADS "Search for a threading library to build with multithreading support" ON)
option(PREFER_WIN32THREADS "When searching for a threading library, prefer win32 threads if they are found" OFF)
option(WITH_GNUTLS "Search for the GnuTLS secure communications library to support TLS" ON)
option(WITH_OPENSSL "Search for the OpenSSL cryptography library to support TLS and use as crypto backend" ON)
option(WITH_SYSTEMD "Search for libsystemd to build with systemd socket activation support" ON)
option(WITH_GCRYPT "Search for Libgcrypt to use as crypto backend" ON)
option(WITH_FFMPEG "Search for FFMPEG to build an example VNC to MPEG encoder" ON)
option(WITH_TIGHTVNC_FILETRANSFER "Enable filetransfer if there is pthreads support" ON)
option(WITH_24BPP "Allow 24 bpp" ON)
option(WITH_IPv6 "Enable IPv6 Support" ON)
option(WITH_WEBSOCKETS "Build with websockets support" ON)
option(WITH_SASL "Build with SASL support" ON)
option(WITH_XCB "Build with XCB support" ON)
option(WITH_EXAMPLES "Build examples" ON)
option(WITH_TESTS "Build tests" ON)



if(WITH_ZLIB)
  find_package(ZLIB)
endif(WITH_ZLIB)

if(WITH_LZO)
  find_package(LZO)
endif()

if(WITH_XCB)
  find_package(X11) # Need CMake 3.24.0 to find XCB libraries. see https://cmake.org/cmake/help/v3.24/module/FindX11.html 
endif()

if(WITH_JPEG)
  find_package(JPEG)
  if(JPEG_FOUND)
    # Check whether the version of libjpeg we found was libjpeg-turbo and print a
    # warning if not.
    set(CMAKE_REQUIRED_LIBRARIES ${JPEG_LIBRARIES})
    if(JPEG_INCLUDE_DIRS) # this was not present in 3.4
      set(CMAKE_REQUIRED_INCLUDES ${JPEG_INCLUDE_DIRS})
    else()
      set(CMAKE_REQUIRED_INCLUDES ${JPEG_INCLUDE_DIR})
    endif()

    set(JPEG_TEST_SOURCE "\n
    #include <stdio.h>\n
    #include <jpeglib.h>\n
    int main(void) {\n
      struct jpeg_compress_struct cinfo;\n
      struct jpeg_error_mgr jerr;\n
      cinfo.err=jpeg_std_error(&jerr);\n
      jpeg_create_compress(&cinfo);\n
      cinfo.input_components = 3;\n
      jpeg_set_defaults(&cinfo);\n
      cinfo.in_color_space = JCS_EXT_RGB;\n
      jpeg_default_colorspace(&cinfo);\n
      return 0;\n
    }")

    if(CMAKE_CROSSCOMPILING)
      check_c_source_compiles("${JPEG_TEST_SOURCE}" FOUND_LIBJPEG_TURBO)
    else()
      check_c_source_runs("${JPEG_TEST_SOURCE}" FOUND_LIBJPEG_TURBO)
    endif()

    set(CMAKE_REQUIRED_LIBRARIES)
    set(CMAKE_REQUIRED_INCLUDES)
    set(CMAKE_REQUIRED_DEFINITIONS)

    if(NOT FOUND_LIBJPEG_TURBO)
      # last-resort grep check (when using LibVNCServer as a CMake subdir together with turbojpeg CMake subdir, the build check above fails since turbojpeg is not yet built when LibVNCServer is configured)
      get_filename_component(JPEGLIB_H_PATH "${JPEG_INCLUDE_DIR}/jpeglib.h" REALPATH BASE_DIR "${CMAKE_SOURCE_DIR}")
      message(STATUS "Runtime check for libjpeg-turbo failed, inspecting ${JPEGLIB_H_PATH}")
      file(STRINGS ${JPEGLIB_H_PATH} FOUND_LIBJPEG_TURBO REGEX "JCS_EXT_RGB")
      if(NOT FOUND_LIBJPEG_TURBO)
	message(WARNING "*** The libjpeg library you are building against is not libjpeg-turbo.  Performance will be reduced.  You can obtain libjpeg-turbo from:  https://sourceforge.net/projects/libjpeg-turbo/files/ ***")
      endif()
    endif()
  endif(JPEG_FOUND)
endif(WITH_JPEG)


if(WITH_PNG)
  find_package(PNG)
endif(WITH_PNG)


if(WITH_SDL)
  find_package(SDL2)
endif(WITH_SDL)

if(WITH_GTK)
  find_package(GTK2)
endif(WITH_GTK)

if(WITH_LIBSSH2)
  find_package(LibSSH2)
endif(WITH_LIBSSH2)

if(WITH_THREADS)
  find_package(Threads)
endif(WITH_THREADS)


if(WITH_GNUTLS)
  find_package(GnuTLS 3.4.0)
endif(WITH_GNUTLS)


if(WITH_OPENSSL)
  find_package(OpenSSL)
endif(WITH_OPENSSL)


if(WITH_SYSTEMD AND NOT ANDROID AND NOT WIN32)
  find_package(PkgConfig)
  pkg_check_modules(SYSTEMD "libsystemd")
endif(WITH_SYSTEMD AND NOT ANDROID AND NOT WIN32)


if(WITH_GCRYPT)
  find_library(LIBGCRYPT_LIBRARIES gcrypt)
endif(WITH_GCRYPT)

if(WITH_FFMPEG)
  find_package(FFMPEG 3.1.0 COMPONENTS avformat avcodec avutil swscale)
endif(WITH_FFMPEG)


check_include_file("dirent.h"      LIBVNCSERVER_HAVE_DIRENT_H)
check_include_file("endian.h"      LIBVNCSERVER_HAVE_ENDIAN_H)
check_include_file("fcntl.h"       LIBVNCSERVER_HAVE_FCNTL_H)
check_include_file("netinet/in.h"  LIBVNCSERVER_HAVE_NETINET_IN_H)
check_include_file("sys/endian.h"  LIBVNCSERVER_HAVE_SYS_ENDIAN_H)
check_include_file("sys/socket.h"  LIBVNCSERVER_HAVE_SYS_SOCKET_H)
check_include_file("sys/stat.h"    LIBVNCSERVER_HAVE_SYS_STAT_H)
check_include_file("sys/time.h"    LIBVNCSERVER_HAVE_SYS_TIME_H)
check_include_file("sys/types.h"   LIBVNCSERVER_HAVE_SYS_TYPES_H)
check_include_file("sys/wait.h"    LIBVNCSERVER_HAVE_SYS_WAIT_H)
check_include_file("unistd.h"      LIBVNCSERVER_HAVE_UNISTD_H)
check_include_file("sys/resource.h"     LIBVNCSERVER_HAVE_SYS_RESOURCE_H)


# headers needed for check_type_size()
check_include_file("vfork.h"       LIBVNCSERVER_HAVE_VFORK_H)
check_include_file("ws2tcpip.h"    LIBVNCSERVER_HAVE_WS2TCPIP_H)
check_include_file("arpa/inet.h"   HAVE_ARPA_INET_H)
check_include_file("stdint.h"      HAVE_STDINT_H)
check_include_file("stddef.h"      HAVE_STDDEF_H)
check_include_file("sys/types.h"   HAVE_SYS_TYPES_H)

# error out if required headers not found
if(NOT HAVE_STDINT_H)
  message(FATAL_ERROR "Could NOT find required header stdint.h")
endif()

check_function_exists(gettimeofday    LIBVNCSERVER_HAVE_GETTIMEOFDAY)
check_function_exists(vfork           LIBVNCSERVER_HAVE_VFORK)
check_function_exists(vprintf         LIBVNCSERVER_HAVE_VPRINTF)
check_function_exists(mmap            LIBVNCSERVER_HAVE_MMAP)
check_function_exists(fork            LIBVNCSERVER_HAVE_FORK)
check_function_exists(ftime           LIBVNCSERVER_HAVE_FTIME)
check_function_exists(gethostbyname   LIBVNCSERVER_HAVE_GETHOSTBYNAME)
check_function_exists(gethostname     LIBVNCSERVER_HAVE_GETHOSTNAME)
check_function_exists(inet_ntoa       LIBVNCSERVER_HAVE_INET_NTOA)
check_function_exists(memmove         LIBVNCSERVER_HAVE_MEMMOVE)
check_function_exists(memset          LIBVNCSERVER_HAVE_MEMSET)
check_function_exists(mkfifo          LIBVNCSERVER_HAVE_MKFIFO)
check_function_exists(select          LIBVNCSERVER_HAVE_SELECT)
check_function_exists(socket          LIBVNCSERVER_HAVE_SOCKET)
check_function_exists(strchr          LIBVNCSERVER_HAVE_STRCHR)
check_function_exists(strcspn         LIBVNCSERVER_HAVE_STRCSPN)
check_function_exists(strdup          LIBVNCSERVER_HAVE_STRDUP)
check_function_exists(strerror        LIBVNCSERVER_HAVE_STRERROR)
check_function_exists(strstr          LIBVNCSERVER_HAVE_STRSTR)

check_symbol_exists(htobe64 "endian.h" LIBVNCSERVER_HAVE_HTOBE64)
check_symbol_exists(htobe64 "sys/endian.h" LIBVNCSERVER_HAVE_HTOBE64)
check_symbol_exists(OSSwapHostToBigInt64 "libkern/OSByteOrder.h" LIBVNCSERVER_HAVE_OSSWAPHOSTTOBIGINT64)

if(WITH_THREADS AND Threads_FOUND)
  set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif(WITH_THREADS AND Threads_FOUND)
if(ZLIB_FOUND)
  set(LIBVNCSERVER_HAVE_LIBZ 1)
else()
  unset(ZLIB_LIBRARIES) # would otherwise contain -NOTFOUND, confusing target_link_libraries()
endif(ZLIB_FOUND)
if(LZO_FOUND)
  set(LIBVNCSERVER_HAVE_LZO 1)
else()
  unset(LZO_LIBRARIES CACHE) # would otherwise contain -NOTFOUND, confusing target_link_libraries()
endif()
if(JPEG_FOUND)
  set(LIBVNCSERVER_HAVE_LIBJPEG 1)
else()
  unset(JPEG_LIBRARIES) # would otherwise confuse target_link_libraries()
endif(JPEG_FOUND)
if(PNG_FOUND)
  set(LIBVNCSERVER_HAVE_LIBPNG 1)
else()
  unset(PNG_LIBRARIES) # would otherwise contain -NOTFOUND, confusing target_link_libraries()
endif(PNG_FOUND)
if(NOT OPENSSL_FOUND)
    unset(OPENSSL_LIBRARIES) # would otherwise contain -NOTFOUND, confusing target_link_libraries()
endif()
if(SYSTEMD_FOUND)
  add_definitions(-DLIBVNCSERVER_WITH_SYSTEMD)
  include_directories(${SYSTEMD_INCLUDE_DIRS})
  set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ${SYSTEMD_LIBRARIES})
endif(SYSTEMD_FOUND)

# common crypto used by both libvncserver and libvncclient
if(WITH_GCRYPT AND LIBGCRYPT_LIBRARIES)
  message(STATUS "Building crypto with Libgcrypt")
  set(CRYPTO_LIBRARIES ${LIBGCRYPT_LIBRARIES})
  set(CRYPTO_SOURCES ${COMMON_DIR}/crypto_libgcrypt.c)
elseif(OPENSSL_FOUND)
  message(STATUS "Building crypto with OpenSSL")
  set(CRYPTO_LIBRARIES ${OPENSSL_LIBRARIES})
  set(CRYPTO_SOURCES ${COMMON_DIR}/crypto_openssl.c)
else()
  message(STATUS "Building crypto with builtin functions, only including SHA1 and D3DES")
  set(CRYPTO_SOURCES ${COMMON_DIR}/crypto_included.c ${COMMON_DIR}/sha1.c ${COMMON_DIR}/d3des.c)
endif()

if(WITH_WEBSOCKETS AND (LIBVNCSERVER_HAVE_HTOBE64 OR LIBVNCSERVER_HAVE_OSSWAPHOSTTOBIGINT64))
  set(LIBVNCSERVER_WITH_WEBSOCKETS 1)
endif()

if(WITH_GCRYPT AND LIBGCRYPT_LIBRARIES)
  set(LIBVNCSERVER_HAVE_LIBGCRYPT 1)
endif(WITH_GCRYPT AND LIBGCRYPT_LIBRARIES)

if(GNUTLS_FOUND)
  set(LIBVNCSERVER_HAVE_GNUTLS 1)
endif(GNUTLS_FOUND)

if(OPENSSL_FOUND)
  include_directories(${OPENSSL_INCLUDE_DIR})
  set(LIBVNCSERVER_HAVE_LIBSSL 1)
endif(OPENSSL_FOUND)

if(WITH_IPv6)
  if(WIN32 AND LIBVNCSERVER_HAVE_WS2TCPIP_H AND LIBVNCSERVER_HAVE_VPRINTF)
    set(LIBVNCSERVER_IPv6 1)
  endif()
  if(NOT WIN32)
    set(LIBVNCSERVER_IPv6 1)
  endif()
endif(WITH_IPv6)


if(WITH_24BPP)
  set(LIBVNCSERVER_ALLOW24BPP 1)
endif()

# Make sure that only one threading system is used. This happens on MinGW.
if(WITH_THREADS)
  if(CMAKE_USE_PTHREADS_INIT AND CMAKE_USE_WIN32_THREADS_INIT)
    if(PREFER_WIN32THREADS)
      unset(CMAKE_USE_PTHREADS_INIT)
    else()
      unset(CMAKE_USE_WIN32_THREADS_INIT)
    endif(PREFER_WIN32THREADS)
  endif(CMAKE_USE_PTHREADS_INIT AND CMAKE_USE_WIN32_THREADS_INIT)

  if(CMAKE_USE_PTHREADS_INIT)
    set(LIBVNCSERVER_HAVE_LIBPTHREAD 1)
    message(STATUS "Threads support is using pthreads")
  endif(CMAKE_USE_PTHREADS_INIT)

  if(CMAKE_USE_WIN32_THREADS_INIT)
    set(LIBVNCSERVER_HAVE_WIN32THREADS 1)
    message(STATUS "Threads support is using win32 threads")
  endif(CMAKE_USE_WIN32_THREADS_INIT)
endif(WITH_THREADS)

if(LIBVNCSERVER_HAVE_SYS_SOCKET_H)
  # socklen_t
  list(APPEND CMAKE_EXTRA_INCLUDE_FILES "sys/socket.h")
endif(LIBVNCSERVER_HAVE_SYS_SOCKET_H)
if(HAVE_ARPA_INET_H)
  # in_addr_t
  list(APPEND CMAKE_EXTRA_INCLUDE_FILES "arpa/inet.h")
endif(HAVE_ARPA_INET_H)

check_type_size(pid_t     LIBVNCSERVER_PID_T)
check_type_size(size_t    LIBVNCSERVER_SIZE_T)
check_type_size(socklen_t LIBVNCSERVER_SOCKLEN_T)
check_type_size(in_addr_t LIBVNCSERVER_IN_ADDR_T)
if(NOT HAVE_LIBVNCSERVER_IN_ADDR_T)
  set(LIBVNCSERVER_NEED_INADDR_T 1)
endif(NOT HAVE_LIBVNCSERVER_IN_ADDR_T)

TEST_BIG_ENDIAN(LIBVNCSERVER_WORDS_BIGENDIAN)

if(WITH_SASL)
  find_path(SASL2_INCLUDE_DIR sasl/sasl.h)
  find_library(LIBSASL2_LIBRARIES sasl2 libsasl.lib)
endif(WITH_SASL)

if(WITH_SASL AND LIBSASL2_LIBRARIES AND SASL2_INCLUDE_DIR)
  message(STATUS "Building with SASL: ${LIBSASL2_LIBRARIES} and ${SASL2_INCLUDE_DIR}")
  set(LIBVNCSERVER_HAVE_SASL 1)
  set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ${LIBSASL2_LIBRARIES})
  include_directories(${SASL2_INCLUDE_DIR})
endif(WITH_SASL AND LIBSASL2_LIBRARIES AND SASL2_INCLUDE_DIR)

# TODO:
# LIBVNCSERVER_ENOENT_WORKAROUND
# inline

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/rfb/rfbconfig.h.cmakein ${CMAKE_CURRENT_BINARY_DIR}/include/rfb/rfbconfig.h)

set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_DIR}/main.c
    ${LIBVNCSERVER_DIR}/rfbserver.c
    ${LIBVNCSERVER_DIR}/rfbregion.c
    ${LIBVNCSERVER_DIR}/auth.c
    ${LIBVNCSERVER_DIR}/sockets.c
    ${LIBVNCSERVER_DIR}/stats.c
    ${LIBVNCSERVER_DIR}/corre.c
    ${LIBVNCSERVER_DIR}/hextile.c
    ${LIBVNCSERVER_DIR}/rre.c
    ${LIBVNCSERVER_DIR}/translate.c
    ${LIBVNCSERVER_DIR}/cutpaste.c
    ${LIBVNCSERVER_DIR}/httpd.c
    ${LIBVNCSERVER_DIR}/cursor.c
    ${LIBVNCSERVER_DIR}/font.c
    ${LIBVNCSERVER_DIR}/draw.c
    ${LIBVNCSERVER_DIR}/selbox.c
    ${COMMON_DIR}/vncauth.c
    ${COMMON_DIR}/sockets.c
    ${LIBVNCSERVER_DIR}/cargs.c
    ${LIBVNCSERVER_DIR}/ultra.c
    ${LIBVNCSERVER_DIR}/scale.c
    ${CRYPTO_SOURCES}
)

set(LIBVNCCLIENT_SOURCES
    ${LIBVNCCLIENT_DIR}/cursor.c
    ${LIBVNCCLIENT_DIR}/listen.c
    ${LIBVNCCLIENT_DIR}/rfbclient.c
    ${LIBVNCCLIENT_DIR}/sockets.c
    ${LIBVNCCLIENT_DIR}/vncviewer.c
    ${COMMON_DIR}/sockets.c
    ${CRYPTO_SOURCES}
)

if(JPEG_FOUND)
  set(LIBVNCCLIENT_SOURCES
    ${LIBVNCCLIENT_SOURCES}
    ${COMMON_DIR}/turbojpeg.c
  )
endif()

if(GNUTLS_FOUND)
  message(STATUS "Building TLS with GnuTLS")
  set(LIBVNCCLIENT_SOURCES
    ${LIBVNCCLIENT_SOURCES}
    ${LIBVNCCLIENT_DIR}/tls_gnutls.c
  )
  set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${LIBVNCSERVER_DIR}/rfbssl_gnutls.c
    )
  include_directories(${GNUTLS_INCLUDE_DIR})
elseif(OPENSSL_FOUND)
  message(STATUS "Building TLS with OpenSSL")
  set(LIBVNCCLIENT_SOURCES
    ${LIBVNCCLIENT_SOURCES}
    ${LIBVNCCLIENT_DIR}/tls_openssl.c
  )
  set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${LIBVNCSERVER_DIR}/rfbssl_openssl.c
  )
  include_directories(${OPENSSL_INCLUDE_DIR})
else()
  message(STATUS "Building without TLS")
  set(LIBVNCCLIENT_SOURCES
    ${LIBVNCCLIENT_SOURCES}
    ${LIBVNCCLIENT_DIR}/tls_none.c
  )
  set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${LIBVNCSERVER_DIR}/rfbssl_none.c
  )
endif()

if(LIBVNCSERVER_HAVE_SASL)
  set(LIBVNCCLIENT_SOURCES
    ${LIBVNCCLIENT_SOURCES}
    ${LIBVNCCLIENT_DIR}/sasl.c
  )
endif()

if(ZLIB_FOUND)
  add_definitions(-DLIBVNCSERVER_HAVE_LIBZ)
  include_directories(${ZLIB_INCLUDE_DIR})
  set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${LIBVNCSERVER_DIR}/zlib.c
    ${LIBVNCSERVER_DIR}/zrle.c
    ${LIBVNCSERVER_DIR}/zrleoutstream.c
    ${LIBVNCSERVER_DIR}/zrlepalettehelper.c
  )
endif(ZLIB_FOUND)

if(LZO_FOUND)
  add_definitions(-DLIBVNCSERVER_HAVE_LZO)
  include_directories(${LZO_INCLUDE_DIR})
else()
  set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${COMMON_DIR}/minilzo.c
  )
  set(LIBVNCCLIENT_SOURCES
    ${LIBVNCCLIENT_SOURCES}
    ${COMMON_DIR}/minilzo.c
  )
endif()

if(JPEG_FOUND)
  add_definitions(-DLIBVNCSERVER_HAVE_LIBJPEG)
  include_directories(${JPEG_INCLUDE_DIR})
  if(PNG_FOUND OR ZLIB_FOUND)
    set(TIGHT_C ${LIBVNCSERVER_DIR}/tight.c ${COMMON_DIR}/turbojpeg.c)
  endif(PNG_FOUND OR ZLIB_FOUND)
endif(JPEG_FOUND)

if(PNG_FOUND)
  add_definitions(-DLIBVNCSERVER_HAVE_LIBPNG)
  include_directories(${PNG_INCLUDE_DIR})
endif(PNG_FOUND)

set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${TIGHT_C}
)

if(WITH_THREADS AND WITH_TIGHTVNC_FILETRANSFER AND CMAKE_USE_PTHREADS_INIT)
  set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${LIBVNCSERVER_DIR}/tightvnc-filetransfer/rfbtightserver.c
    ${LIBVNCSERVER_DIR}/tightvnc-filetransfer/handlefiletransferrequest.c
    ${LIBVNCSERVER_DIR}/tightvnc-filetransfer/filetransfermsg.c
    ${LIBVNCSERVER_DIR}/tightvnc-filetransfer/filelistinfo.c
  )
endif(WITH_THREADS AND WITH_TIGHTVNC_FILETRANSFER AND CMAKE_USE_PTHREADS_INIT)

if(LIBVNCSERVER_WITH_WEBSOCKETS)
  add_definitions(-DLIBVNCSERVER_WITH_WEBSOCKETS)
  set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${LIBVNCSERVER_DIR}/websockets.c
    ${LIBVNCSERVER_DIR}/ws_decode.c
    ${COMMON_DIR}/base64.c
  )
endif(LIBVNCSERVER_WITH_WEBSOCKETS)

add_library(vncclient ${LIBVNCCLIENT_SOURCES})
add_library(vncserver ${LIBVNCSERVER_SOURCES})
if(WIN32)
  set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ws2_32)
  if(WITH_TIGHTVNC_FILETRANSFER)
    add_definitions(-D_WIN32_WINNT=0x0600)
  endif(WITH_TIGHTVNC_FILETRANSFER)
endif(WIN32)

target_link_libraries(vncclient
                      ${ADDITIONAL_LIBS}
                      ${ZLIB_LIBRARIES}
                      ${LZO_LIBRARIES}
                      ${JPEG_LIBRARIES}
		      ${CRYPTO_LIBRARIES}
                      ${GNUTLS_LIBRARIES}
                      ${OPENSSL_LIBRARIES}
)
target_link_libraries(vncserver
                      ${ADDITIONAL_LIBS}
                      ${ZLIB_LIBRARIES}
                      ${LZO_LIBRARIES}
                      ${JPEG_LIBRARIES}
		      ${PNG_LIBRARIES}
		      ${CRYPTO_LIBRARIES}
                      ${GNUTLS_LIBRARIES}
                      ${OPENSSL_LIBRARIES}
)

SET_TARGET_PROPERTIES(vncclient vncserver
		PROPERTIES SOVERSION "${VERSION_SO}" VERSION "${LibVNCServer_VERSION}" C_STANDARD 90
)

# EXAMPLES
set(LIBVNCSERVER_EXAMPLES
    backchannel
    camera
    cursors
    colourmaptest
    example
    fontsel
    pnmshow
    pnmshow24
    regiontest
    repeater
    rotate
    simple
    simple15
    storepasswd
    vncev
  )

if(WITH_THREADS AND (CMAKE_USE_PTHREADS_INIT OR CMAKE_USE_WIN32_THREADS_INIT))
  set(LIBVNCSERVER_EXAMPLES
    ${LIBVNCSERVER_EXAMPLES}
    blooptest
  )
endif(WITH_THREADS AND (CMAKE_USE_PTHREADS_INIT OR CMAKE_USE_WIN32_THREADS_INIT))

if(WITH_THREADS AND WITH_TIGHTVNC_FILETRANSFER AND CMAKE_USE_PTHREADS_INIT)
  set(LIBVNCSERVER_EXAMPLES
    ${LIBVNCSERVER_EXAMPLES}
    filetransfer
  )
endif(WITH_THREADS AND WITH_TIGHTVNC_FILETRANSFER AND CMAKE_USE_PTHREADS_INIT)

if(APPLE AND NOT IOS AND WITH_THREADS AND CMAKE_USE_PTHREADS_INIT)
  set(LIBVNCSERVER_EXAMPLES
    ${LIBVNCSERVER_EXAMPLES}
    mac
  )
  find_library(CARBON_LIBRARY Carbon)
  find_library(IOKIT_LIBRARY IOKit)
  find_library(IOSURFACE_LIBRARY IOSurface)
endif(APPLE AND NOT IOS AND WITH_THREADS AND CMAKE_USE_PTHREADS_INIT)

if(ANDROID)
  set(LIBVNCSERVER_EXAMPLES
    ${LIBVNCSERVER_EXAMPLES}
    androidvncserver
  )
endif(ANDROID)

if(X11_xcb_FOUND AND X11_xcb_xtest_FOUND AND X11_xcb_keysyms_FOUND)
  set(LIBVNCSERVER_EXAMPLES
    ${LIBVNCSERVER_EXAMPLES}
    x11
  )
else()
  # clear NOTFOUND
  unset(X11_xcb_LIB CACHE)
  unset(X11_xcb_xtest_LIB CACHE)
  unset(X11_xcb_keysyms_LIB CACHE)
endif(X11_xcb_FOUND AND X11_xcb_xtest_FOUND AND X11_xcb_keysyms_FOUND)

set(LIBVNCCLIENT_EXAMPLES
    backchannel
    ppmtest
)

if(SDL2_FOUND)
  include_directories(${SDL2_INCLUDE_DIR})
  set(LIBVNCCLIENT_EXAMPLES
    ${LIBVNCCLIENT_EXAMPLES}
    SDLvncviewer
  )
endif(SDL2_FOUND)

if(GTK2_FOUND)
  include_directories(${GTK2_INCLUDE_DIRS})
  set(LIBVNCCLIENT_EXAMPLES
    ${LIBVNCCLIENT_EXAMPLES}
    gtkvncviewer
  )
endif(GTK2_FOUND)

if(LIBSSH2_FOUND AND (CMAKE_USE_PTHREADS_INIT OR CMAKE_USE_WIN32_THREADS_INIT))
  include_directories(${LIBSSH2_INCLUDE_DIR})
  set(LIBVNCCLIENT_EXAMPLES
    ${LIBVNCCLIENT_EXAMPLES}
    sshtunnel
  )
endif()
# if not found, set lib var to empty, otherwise CMake complains
if(NOT LIBSSH2_FOUND)
  set(LIBSSH2_LIBRARY "")
endif()

if(FFMPEG_FOUND)
  set(LIBVNCCLIENT_EXAMPLES
    ${LIBVNCCLIENT_EXAMPLES}
    vnc2mpg
  )
endif(FFMPEG_FOUND)

if(WITH_EXAMPLES)
  foreach(e ${LIBVNCSERVER_EXAMPLES})
    add_executable(examples_${e} ${LIBVNCSRVEXAMPLE_DIR}/${e}.c)
    set_target_properties(examples_${e} PROPERTIES OUTPUT_NAME ${e})
    set_target_properties(examples_${e} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/examples/server)
    target_link_libraries(examples_${e} vncserver ${CMAKE_THREAD_LIBS_INIT} ${CARBON_LIBRARY} ${IOKIT_LIBRARY} ${IOSURFACE_LIBRARY} ${X11_xcb_LIB} ${X11_xcb_xtest_LIB} ${X11_xcb_keysyms_LIB})
  endforeach(e ${LIBVNCSERVER_EXAMPLES})

  foreach(e ${LIBVNCCLIENT_EXAMPLES})
    add_executable(client_examples_${e} ${LIBVNCCLIEXAMPLE_DIR}/${e}.c ${LIBVNCCLIEXAMPLE_DIR}/${${e}_EXTRA_SOURCES} )
    set_target_properties(client_examples_${e} PROPERTIES OUTPUT_NAME ${e})
    set_target_properties(client_examples_${e} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/examples/client)
    target_link_libraries(client_examples_${e} vncclient ${CMAKE_THREAD_LIBS_INIT} ${SDL2_LIBRARY} ${GTK2_LIBRARIES} ${FFMPEG_LIBRARIES} ${LIBSSH2_LIBRARY})
  endforeach(e ${LIBVNCCLIENT_EXAMPLES})
endif(WITH_EXAMPLES)

#
# them tests
#

if(WITH_TESTS)

# First fuzzing
if(DEFINED ENV{LIB_FUZZING_ENGINE})
  add_executable(fuzz_server ${TESTS_DIR}/fuzz_server.c)
  target_link_libraries(fuzz_server vncserver ${CMAKE_THREAD_LIBS_INIT} ${CARBON_LIBRARY} ${IOKIT_LIBRARY} ${IOSURFACE_LIBRARY} $ENV{LIB_FUZZING_ENGINE})
endif()

if(UNIX)
    set(ADDITIONAL_TEST_LIBS m)
endif(UNIX)

set(SIMPLETESTS
   cargstest
   copyrecttest
)

if(WITH_THREADS AND (CMAKE_USE_PTHREADS_INIT OR CMAKE_USE_WIN32_THREADS_INIT))
  set(SIMPLETESTS
      ${SIMPLETESTS}
      encodingstest
     )
endif(WITH_THREADS AND (CMAKE_USE_PTHREADS_INIT OR CMAKE_USE_WIN32_THREADS_INIT))

foreach(t ${SIMPLETESTS})
  add_executable(test_${t} ${TESTS_DIR}/${t}.c)
  set_target_properties(test_${t} PROPERTIES OUTPUT_NAME ${t})
  set_target_properties(test_${t} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test)
  target_link_libraries(test_${t} vncserver vncclient ${ADDITIONAL_TEST_LIBS})
endforeach(t ${SIMPLETESTS})

if(WITH_JPEG AND FOUND_LIBJPEG_TURBO)
  add_executable(test_tjunittest
                 ${TESTS_DIR}/tjunittest.c
                 ${TESTS_DIR}/tjutil.c
                 ${TESTS_DIR}/tjutil.h
                 ${COMMON_DIR}/turbojpeg.c
                 ${COMMON_DIR}/turbojpeg.h
                )
  set_target_properties(test_tjunittest PROPERTIES OUTPUT_NAME tjunittest)
  set_target_properties(test_tjunittest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test)
  target_link_libraries(test_tjunittest vncserver vncclient ${ADDITIONAL_TEST_LIBS})

  add_executable(test_tjbench
                 ${TESTS_DIR}/tjbench.c
                 ${TESTS_DIR}/tjutil.c
                 ${TESTS_DIR}/tjutil.h
                 ${TESTS_DIR}/bmp.c
                 ${TESTS_DIR}/bmp.h
                 ${COMMON_DIR}/turbojpeg.c
                 ${COMMON_DIR}/turbojpeg.h
                )
  set_target_properties(test_tjbench PROPERTIES OUTPUT_NAME tjbench)
  set_target_properties(test_tjbench PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test)
  target_link_libraries(test_tjbench vncserver vncclient ${ADDITIONAL_TEST_LIBS})

endif(WITH_JPEG AND FOUND_LIBJPEG_TURBO)

if(LIBVNCSERVER_WITH_WEBSOCKETS)
  add_executable(test_wstest
    ${TESTS_DIR}/wstest.c
    ${TESTS_DIR}/wstestdata.inc
    )
  set_target_properties(test_wstest PROPERTIES OUTPUT_NAME wstest)
  set_target_properties(test_wstest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test)
  target_link_libraries(test_wstest vncserver vncclient ${ADDITIONAL_TEST_LIBS})
endif(LIBVNCSERVER_WITH_WEBSOCKETS)

add_test(NAME cargs COMMAND test_cargstest)
if(UNIX)
  add_test(NAME includetest COMMAND ${TESTS_DIR}/includetest.sh ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR} ${CMAKE_MAKE_PROGRAM})
endif(UNIX)
if(WITH_JPEG AND FOUND_LIBJPEG_TURBO)
    add_test(NAME turbojpeg COMMAND test_tjunittest)
endif(WITH_JPEG AND FOUND_LIBJPEG_TURBO)
if(LIBVNCSERVER_WITH_WEBSOCKETS)
    add_test(NAME wstest COMMAND test_wstest)
endif(LIBVNCSERVER_WITH_WEBSOCKETS)

endif(WITH_TESTS)

#
# this gets the libraries needed by TARGET in "-libx -liby ..." form
#
function(get_link_libraries OUT TARGET)
    set(RESULT "")
    get_target_property(LIBRARIES ${TARGET} INTERFACE_LINK_LIBRARIES)
    foreach(LIB ${LIBRARIES})
	if("${LIB}" MATCHES ".*NOTFOUND.*")
	    continue()
	endif()
	string(REGEX REPLACE "^.*/lib" "" LIB ${LIB}) # remove leading path and "lib" name prefix
	string(REGEX REPLACE "-l" "" LIB ${LIB}) # remove leading -l
	string(REGEX REPLACE "\\.so$" "" LIB ${LIB}) # remove trailing .so
	list(APPEND RESULT "-l${LIB}")
    endforeach()
    list(REMOVE_DUPLICATES RESULT)
    string(CONCAT RESULT ${RESULT}) # back to string
    if(RESULT)
	string(REPLACE "-l" " -l" RESULT ${RESULT}) # re-add separators
    endif(RESULT)
    set(${OUT} ${RESULT} PARENT_SCOPE)
endfunction()

get_link_libraries(PRIVATE_LIBS vncserver)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/libvncserver/libvncserver.pc.cmakein ${CMAKE_CURRENT_BINARY_DIR}/libvncserver.pc @ONLY)
get_link_libraries(PRIVATE_LIBS vncclient)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/libvncclient/libvncclient.pc.cmakein ${CMAKE_CURRENT_BINARY_DIR}/libvncclient.pc @ONLY)

if(LIBVNCSERVER_INSTALL)
    set(INSTALL_HEADER_FILES
        include/rfb/keysym.h
        include/rfb/threading.h
        include/rfb/rfb.h
        include/rfb/rfbclient.h
        ${CMAKE_CURRENT_BINARY_DIR}/include/rfb/rfbconfig.h
        include/rfb/rfbproto.h
        include/rfb/rfbregion.h
        )
    
    set_property(TARGET vncclient PROPERTY PUBLIC_HEADER ${INSTALL_HEADER_FILES})
    set_property(TARGET vncserver PROPERTY PUBLIC_HEADER ${INSTALL_HEADER_FILES})
    if(WIN32)
        INSTALL(TARGETS vncclient vncserver
            EXPORT LibVNCServerTargets
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rfb
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            )
    else()
        INSTALL(TARGETS vncclient vncserver
            EXPORT LibVNCServerTargets
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rfb
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            )
    endif()
    
    # Install cmake configure files
    install(EXPORT LibVNCServerTargets
        NAMESPACE
            "LibVNCServer::"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LibVNCServer"
        )
    
    include(CMakePackageConfigHelpers)
    configure_package_config_file(
      ${PROJECT_SOURCE_DIR}/cmake/Modules/LibVNCServerConfig.cmake.in
      ${CMAKE_BINARY_DIR}/LibVNCServerConfig.cmake
      INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LibVNCServer"
      )
    # Install cmake version configure file
    write_basic_package_version_file(
        "${CMAKE_BINARY_DIR}/LibVNCServerConfigVersion.cmake"
        VERSION ${PARA_VERSION}
        COMPATIBILITY AnyNewerVersion)
    install(FILES
        "${CMAKE_BINARY_DIR}/LibVNCServerConfigVersion.cmake"
        "${CMAKE_BINARY_DIR}/LibVNCServerConfig.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LibVNCServer")
    
  
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/libvncserver.pc
        ${CMAKE_CURRENT_BINARY_DIR}/libvncclient.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
        )
endif()
