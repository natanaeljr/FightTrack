cmake_minimum_required(VERSION 3.5)
include(ExternalProject)

#########################################################################################
# SAFELIB
#########################################################################################
option(DOWNLOAD_SAFELIB "Download safelib " OFF)

# Try using SAFELIB headers from system
if(NOT DOWNLOAD_SAFELIB)
    find_path(SAFELIB_INCLUDE_DIR "safe/lockable.h")
    if (SAFELIB_INCLUDE_DIR)
        message(STATUS "Looking for safe-lib headers: ${SAFELIB_INCLUDE_DIR} - found")
    else()
        message(STATUS "Looking for safe-lib headers - not found")
        set(DOWNLOAD_SAFELIB ON)
    endif()
endif(NOT DOWNLOAD_SAFELIB)

# Download SAFELIB locally into the binary directory
if(DOWNLOAD_SAFELIB)
    message(STATUS "safe-lib will be downloaded")
    set(SAFELIB_PREFIX ${CMAKE_BINARY_DIR}/safelib)
    set(SAFELIB_INCLUDE_DIR ${SAFELIB_PREFIX}/src/safelib/include)

    # Download and build
    ExternalProject_Add(safelib
        GIT_REPOSITORY "https://github.com/LouisCharlesC/safe"
        GIT_TAG "master"
        PREFIX ${SAFELIB_PREFIX}
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${SAFELIB_PREFIX}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        UPDATE_COMMAND "")
endif(DOWNLOAD_SAFELIB)

# Export Variables
set(SAFELIB_INCLUDE_DIRS ${SAFELIB_INCLUDE_DIR} CACHE STRING "safe-lib Include directories")
set(SAFELIB_DEFINITIONS "" CACHE STRING "safe-lib Definitions")
