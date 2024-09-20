cmake_minimum_required(VERSION 3.10)

project(NativeVNC C CXX ASM)

set(AVNC_EXTERN_DIR ${PROJECT_SOURCE_DIR}/../extern)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared Libs" FORCE)

###############################################################################
# Utilities
###############################################################################

# Make sure given submodule is checked out
macro(avnc_check_submodule name)
    if (NOT EXISTS "${AVNC_EXTERN_DIR}/${name}/CMakeLists.txt")
        message(FATAL_ERROR "git submodule for ${name} is not initialized. Please run 'git submodule update --init'.")
    endif ()
endmacro()

# Required to enable SIMD support on ARM
if (CMAKE_ANDROID_ARCH STREQUAL "arm64")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} --target=aarch64-linux-android${ANDROID_NATIVE_API_LEVEL}")
elseif (CMAKE_ANDROID_ARCH STREQUAL "arm")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} --target=arm-linux-androideabi${ANDROID_NATIVE_API_LEVEL}")
endif ()

###############################################################################
# JPEG
###############################################################################
avnc_check_submodule(libjpeg-turbo)

set(AVNC_LIBJPEG_SRC_DIR ${AVNC_EXTERN_DIR}/libjpeg-turbo)
set(AVNC_LIBJPEG_BUILD_DIR ${CMAKE_BINARY_DIR}/libjpeg-turbo)

add_subdirectory(${AVNC_LIBJPEG_SRC_DIR} ${AVNC_LIBJPEG_BUILD_DIR})

# Set these variables so FindJPEG can find the library
set(JPEG_LIBRARY ${AVNC_LIBJPEG_BUILD_DIR}/libturbojpeg.a)

# Create an interface library to manage include directories
add_library(libjpeg INTERFACE)
target_include_directories(libjpeg INTERFACE ${AVNC_LIBJPEG_SRC_DIR})

###############################################################################
# SSL
###############################################################################
avnc_check_submodule(wolfssl)

set(AVNC_LIBSSL_SRC_DIR ${AVNC_EXTERN_DIR}/wolfssl)
set(AVNC_LIBSSL_BUILD_DIR ${CMAKE_BINARY_DIR}/wolfssl)

# CMake support in wolfSSl is still under development, so we have to
# manually set some flags to enable OpenSSL compatibility layer
set(WOLFSSL_CRL yes)
set(WOLFSSL_DES3 yes)
set(WOLFSSL_CRYPT_TESTS no)
add_definitions(-DOPENSSL_ALL -DOPENSSL_EXTRA -DHAVE_CRL -DHAVE_EX_DATA
        -DHAVE_ANON -DWOLFSSL_AES_DIRECT -DHAVE_AES_ECB -DWOLFSSL_DES_ECB
        -DWC_NO_HARDEN)

add_subdirectory(${AVNC_LIBSSL_SRC_DIR} ${AVNC_LIBSSL_BUILD_DIR})

# Create an interface library to manage include directories
add_library(libssl INTERFACE)
target_include_directories(libssl INTERFACE ${AVNC_LIBSSL_SRC_DIR}/wolfssl)

###############################################################################
# LibVNC
###############################################################################
avnc_check_submodule(libvncserver)

set(AVNC_LIBVNC_SRC_DIR ${AVNC_EXTERN_DIR}/libvncserver)
set(AVNC_LIBVNC_BUILD_DIR ${CMAKE_BINARY_DIR}/libvncserver)

add_subdirectory(${AVNC_LIBVNC_SRC_DIR} ${AVNC_LIBVNC_BUILD_DIR})

# Create an interface library to manage include directories
add_library(libvnc INTERFACE)
target_include_directories(libvnc INTERFACE ${AVNC_LIBVNC_SRC_DIR}/include ${AVNC_LIBVNC_BUILD_DIR}/include)

###############################################################################
# Native VNC
#
# It contains implementation of JNI native methods, some NDK scaffolding and
# some helpers for OpenGL ES rendering. This is the library loaded from Java.
###############################################################################
set(AVNC_NATIVE_SOURCE src/main/cpp/native-vnc.cpp)

add_library(native-vnc SHARED ${AVNC_NATIVE_SOURCE})

# Link libraries
target_link_libraries(native-vnc
        PRIVATE
        libjpeg
        libssl
        libvnc
        vncclient
)

# Link NDK libraries
find_library(LIB_LOG log)
find_library(LIB_GLES GLESv2)

target_link_libraries(native-vnc
        PRIVATE
        ${LIB_LOG}
        ${LIB_GLES}
)

###############################################################################
# Termux X11
#
###############################################################################
set(X11_EXTERN_DIR ${PROJECT_SOURCE_DIR}/src/main/cpp)
add_subdirectory(${X11_EXTERN_DIR})