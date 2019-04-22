cmake_minimum_required(VERSION 3.0)
project(FightTrack)

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
)
target_link_libraries(fighttrack
    ncurses
)
add_dependencies(fighttrack
    ${FIGHTTRACK_DEPENDS}
)

# Executable
add_executable(${PROJECT_NAME}
    src/main.cc
)
target_link_libraries(${PROJECT_NAME}
    fighttrack
)
