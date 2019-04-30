cmake_minimum_required(VERSION 3.0)
project(fight-track)

# General
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-g")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
include(cmake/GSL.cmake)
include(cmake/safelib.cmake)
add_definitions(${GSL_DEFINITIONS})
add_definitions(${SAFELIB_DEFINITIONS})

if(DOWNLOAD_GSL)
    list(APPEND FIGHTTRACK_DEPENDS GSL)
endif()
if(DOWNLOAD_SAFELIB)
    list(APPEND FIGHTTRACK_DEPENDS safelib)
endif()

# Headers
include_directories(include)
include_directories(${GSL_INCLUDE_DIRS})
include_directories(${SAFELIB_INCLUDE_DIRS})

# Library
add_library(fighttrack STATIC
    src/fighttrack.cc
    src/player.cc
    src/player_states.cc
    src/ascii_art.cc
    src/map.cc
    src/server_socket.cc
    src/client_socket.cc
    src/game_client.cc
    src/game_server.cc
)
target_link_libraries(fighttrack
    ncurses
    pthread
)
if(FIGHTTRACK_DEPENDS)
    add_dependencies(fighttrack ${FIGHTTRACK_DEPENDS})
endif()

# Executable
add_executable(${PROJECT_NAME}
    src/main.cc
)
target_link_libraries(${PROJECT_NAME}
    fighttrack
)
