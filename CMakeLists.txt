cmake_minimum_required(VERSION 3.12...3.27)

project(pxlib 
    VERSION 0.6.8
    LANGUAGES C 
    DESCRIPTION "Library to read and write Paradox files"
    HOMEPAGE_URL "https://pxlib.sourceforge.net")

# Source file extensions must be explicit. 
# In CMake 3.19 and below, cmake would implicitly append known extensions to source files if they can't be found.
# In CMake 3.20+, behavior is to not append known extensions and require them to be explicit.
if(POLICY CMP0115) 
    cmake_policy(SET CMP0115 NEW)
endif()

include(FeatureSummary)

option(ENABLE_GSF "Build with gsf support" OFF)


if(CMAKE_COMPILER_IS_GNUCC)
    add_definitions(
        -DHAVE_CONFIG_H
        -Wall -Wpointer-arith -W
        ${PXLIB_EXTRA_GCC_FLAGS}
    )
else()
    add_definitions(-DHAVE_CONFIG_H)
endif()

# Check system for includes
include(CheckIncludeFile)
include(CheckIncludeFiles)
check_include_file("fcntl.h"           HAVE_FCNTL_H)
check_include_file("errno.h"           HAVE_ERRNO_H)
check_include_file("iconv.h"           HAVE_ICONV_H)
check_include_file("inttypes.h"        HAVE_INTTYPES_H)
check_include_file("locale.h"          HAVE_LOCALE_H)
check_include_file("math.h"            HAVE_MATH_H)
check_include_file("stdarg.h"          HAVE_STDARG_H)
check_include_file("stdbool.h"         HAVE_STDBOOL_H)
check_include_file("stddef.h"          HAVE_STDDEF_H)
check_include_file("stdint.h"          HAVE_STDINT_H)
check_include_file("stdlib.h"          HAVE_STDLIB_H)
check_include_file("string.h"          HAVE_STRING_H)
check_include_file("strings.h"         HAVE_STRINGS_H)
check_include_file("sys/time.h"        HAVE_SYS_TIME_H)
check_include_file("sys/types.h"       HAVE_SYS_TYPES_H)
check_include_file("unistd.h"          HAVE_UNISTD_H)
check_include_file("gsf/gsf-input-stdio.h" HAVE_GSF_GSFINPUTSTDIO_H)

check_include_files("stdbool.h;stdio.h;recode.h" HAVE_RECODE LANGUAGE C)

# Functions
include(CheckFunctionExists)
## Checks for library functions.
check_function_exists(strdup HAVE_STRDUP)

# Endianess
include(TestBigEndian)
test_big_endian(WORDS_BIGENDIAN)

find_package(Iconv)

if(HAVE_ICONV_H)
    set(PX_HAVE_ICONV 1)
    set(PX_HAVE_RECODE 0)
else()
    set(PX_HAVE_ICONV 0)
    if(HAVE_RECODE_H)
        set(PX_HAVE_RECODE 1)
    else()
        set(PX_HAVE_RECODE 0)
    endif()
endif()

if(HAVE_GSF_GSFINPUTSTDIO_H)
    set(PX_HAVE_GSF 1)
else()
    set(PX_HAVE_GSF 0)
endif()

set(SOURCES
    src/gregor.c
    src/paradox.c
    src/px_crypt.c
    src/px_encode.c
    src/px_error.c
    src/px_head.c
    src/px_io.c
    src/px_memory.c
    src/px_memprof.c
    src/px_misc.c
    src/fileformat.h
    src/px_crypt.h
    src/px_encode.h
    src/px_error.h
    src/px_head.h
    src/px_intern.h
    src/px_io.h
    src/px_memory.h
    src/px_misc.h
    src/sdncal.h
    include/paradox-gsf.h
    include/paradox.h
    include/paradox-mp.h
    include/pxversion.h
)

add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
add_definitions(-DPXLIB_EXPORTS)

# Versioning
set(PXLIB_MAJOR_VERSION ${pxlib_VERSION_MAJOR})
set(PXLIB_MINOR_VERSION ${pxlib_VERSION_MINOR})
set(PXLIB_MICRO_VERSION ${pxlib_VERSION_PATCH})
set(PXLIB_DOTTED_VERSION "${PXLIB_MAJOR_VERSION}.${PXLIB_MINOR_VERSION}.${PXLIB_MICRO_VERSION}")

# Packaging
set(CPACK_PACKAGE_NAME pxlib)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Reading and writing paradox databases")
set(CPACK_PACKAGE_VENDOR "pxlib")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_VERSION_MAJOR ${PXLIB_MAJOR_VERSION})
set(CPACK_PACKAGE_VERSION_MINOR ${PXLIB_MINOR_VERSION})
set(CPACK_PACKAGE_VERSION_PATCH ${PXLIB_MICRO_VERSION})
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "/usr")
set(CPACK_GENERATOR "TGZ")
set(CPACK_SOURCE_GENERATOR "TGZ")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmakeconfig.h.in config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/pxversion.h.in include/pxversion.h)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/paradox.h.in include/paradox.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/paradox-gsf.h.in include/paradox-gsf.h)



add_library(pxlib SHARED ${SOURCES})

target_include_directories(pxlib
    PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}
    PUBLIC ${CMAKE_BINARY_DIR}/include
)


if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    feature_summary(WHAT ALL)
    feature_summary(FILENAME ${CMAKE_CURRENT_BINARY_DIR}/features.log WHAT ALL)
endif()


include(GNUInstallDirs)
install(TARGETS pxlib LIBRARY)
include(CPack)
