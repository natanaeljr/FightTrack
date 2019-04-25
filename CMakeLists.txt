cmake_minimum_required(VERSION 3.0)
project(fight-track)

# General
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-g")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
include(cmake/GSL.cmake)
add_definitions(${GSL_DEFINITIONS})

if(DOWNLOAD_GSL)
    list(APPEND FIGHTTRACK_DEPENDS GSL)
endif()

# Headers
include_directories(include)
include_directories(${GSL_INCLUDE_DIRS})

# Library
add_library(fighttrack STATIC
    src/game.cc
    src/player.cc
    src/player_states.cc
    src/ascii_art.cc
    src/map.cc
)
target_link_libraries(fighttrack
    ncurses
    pthread
)
if(FIGHTTRACK_DEPENDS)
    add_dependencies(fighttrack
        ${FIGHTTRACK_DEPENDS}
    )
endif()

# Executable
add_executable(${PROJECT_NAME}
    src/main.cc
)
target_link_libraries(${PROJECT_NAME}
    fighttrack
)

# Server
add_executable(server
    src/server.cc
)
target_link_libraries(server
    pthread
)

# Client
add_executable(client
    src/client.cc
)
target_link_libraries(client
    pthread
)