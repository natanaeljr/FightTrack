cmake_minimum_required(VERSION 3.5)
include(ExternalProject)

#########################################################################################
# GSL
#########################################################################################
option(DOWNLOAD_GSL "Download GSL v2.0.0" OFF)

# Try using GSL headers from system
if(NOT DOWNLOAD_GSL)
    find_path(GSL_INCLUDE_DIR "gsl/gsl")
    if (GSL_INCLUDE_DIR)
        message(STATUS "Looking for GSL headers: ${GSL_INCLUDE_DIR} - found")
    else()
        message(STATUS "Looking for GSL headers - not found")
        set(DOWNLOAD_GSL ON)
    endif()
endif(NOT DOWNLOAD_GSL)

# Download GSL locally into the binary directory
if(DOWNLOAD_GSL)
    message(STATUS "GSL will be downloaded")
    set(GSL_PREFIX ${CMAKE_BINARY_DIR}/GSL)
    set(GSL_INCLUDE_DIR ${GSL_PREFIX}/include)

    # Build settings
    set(GSL_TEST OFF)

    # Download and build
    ExternalProject_Add(GSL
        URL "https://github.com/Microsoft/GSL/archive/v2.0.0.zip"
        PREFIX ${GSL_PREFIX}
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${GSL_PREFIX} -DGSL_TEST=${GSL_TEST}
        BUILD_COMMAND ""
        UPDATE_COMMAND "")
endif(DOWNLOAD_GSL)

# Export Variables
set(GSL_INCLUDE_DIRS ${GSL_INCLUDE_DIR} CACHE STRING "GSL Include directories")
set(GSL_DEFINITIONS "" CACHE STRING "GSL Definitions")
