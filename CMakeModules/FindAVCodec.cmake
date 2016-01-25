# - Try to find FFMPEG libavcodec
# Once done, this will define
#
#  AVCodec_FOUND - the library is available
#  AVCodec_INCLUDE_DIRS - the include directories
#  AVCodec_LIBRARIES - the libraries
#  AVCodec_INCLUDE - the file to #include (may be used in config.h)
#
# See documentation on how to write CMake scripts at
# http://www.cmake.org/Wiki/CMake:How_To_Find_Libraries
include ( CheckLibraryExists )
include ( CheckIncludeFile )

find_package ( PkgConfig )
if ( PKG_CONFIG_FOUND )
    pkg_check_modules ( PKGCONFIG_LIBAVCODEC libavcodec )
    pkg_check_modules ( PKGCONFIG_LIBAVFILTER libavfilter )
    pkg_check_modules ( PKGCONFIG_LIBAVSCALER libswscale )
    pkg_check_modules ( PKGCONFIG_LIBAVUTIL libavutil )
endif ( PKG_CONFIG_FOUND )

if ( PKGCONFIG_LIBAVCODEC_FOUND )
    set ( LIBAVCODEC_INCLUDE_DIRS ${PKGCONFIG_LIBAVCODEC_INCLUDE_DIRS} )
    set ( LIBAVCODEC_LIBRARY_DIRS ${PKGCONFIG_LIBAVCODEC_LIBRARIES} )

    foreach ( i ${PKGCONFIG_LIBAVCODEC_LIBRARIES} ${PKGCONFIG_LIBAVUTIL_LIBRARIES} ${PKGCONFIG_LIBAVFILTER_LIBRARIES} ${PKGCONFIG_LIBAVSCALER_LIBRARIES})
        string ( REGEX MATCH "[^-]*" ibase "${i}" )
        find_library ( ${ibase}_LIBRARY
                NAMES ${i}
                PATHS ${PKGCONFIG_LIBAVCODEC_LIBRARY_DIRS}
                )
        if ( ${ibase}_LIBRARY )
            list ( APPEND LIBAVCODEC_LIBRARIES ${${ibase}_LIBRARY} )
        endif ( ${ibase}_LIBRARY )
        mark_as_advanced ( ${ibase}_LIBRARY )
    endforeach ( i )

else ( PKGCONFIG_LIBAVCODEC_FOUND )
    find_file ( LIBAVCODEC_HEADER_FILE
            NAMES
            avcodec.h
            PATHS
            $ENV{LIBAVCODEC_ROOT_DIR}
            PATH_SUFFIXES
            libavcodec
            )
    mark_as_advanced ( LIBAVCODEC_HEADER_FILE )
    get_filename_component ( LIBAVCODEC_INCLUDE_DIRS "${LIBAVCODEC_HEADER_FILE}" PATH )

    find_library ( LIBAVCODEC_LIBRARY
            NAMES
            libavcodec libavutil libavfilter libswscale
            PATHS
            $ENV{LIBAVCODEC_ROOT_DIR}
            PATH_SUFFIXES
            ${LIBAVCODEC_LIBRARY_PATH_SUFFIX}
            )
    mark_as_advanced ( LIBAVCODEC_LIBRARY )
    if ( LIBAVCODEC_LIBRARY )
        set ( LIBAVCODEC_LIBRARIES ${LIBAVCODEC_LIBRARY} )
    endif ( LIBAVCODEC_LIBRARY )

endif ( PKGCONFIG_LIBAVCODEC_FOUND )

if ( LIBAVCODEC_INCLUDE_DIRS AND LIBAVCODEC_LIBRARY_DIRS )
    set ( LIBAVCODEC_FOUND true )
endif ( LIBAVCODEC_INCLUDE_DIRS AND LIBAVCODEC_LIBRARY_DIRS )

if ( LIBAVCODEC_FOUND )
    set ( CMAKE_REQUIRED_INCLUDES "${LIBAVCODEC_INCLUDE_DIRS}" )
    check_include_file ( "{LIBAVCODEC_HEADER_FILE}" LIBAVCODEC_FOUND )
endif ( LIBAVCODEC_FOUND )

if ( LIBAVCODEC_FOUND )
    check_library_exists ( "${LIBAVCODEC_LIBRARY}" avcodec_find_decoder "" LIBAVCODEC_FOUND )
endif ( LIBAVCODEC_FOUND )

if ( NOT LIBAVCODEC_FOUND )
    if ( NOT LIBAVCODEC_FIND_QUIETLY )
        message ( STATUS "LIBAVCODEC not found, try setting LIBAVCODEC_ROOT_DIR environment variable." )
    endif ( NOT LIBAVCODEC_FIND_QUIETLY )
    if ( LIBAVCODEC_FIND_REQUIRED )
        message ( FATAL_ERROR "" )
    endif ( LIBAVCODEC_FIND_REQUIRED )
endif ( NOT LIBAVCODEC_FOUND )

