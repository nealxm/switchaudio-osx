cmake_minimum_required(VERSION 4.0.0)
project(switchaudio-osx LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CONFIGURATION_TYPES Release;Debug)
set(CMAKE_C_FLAGS "-Wall -Wextra -pedantic -Weverything \
-fcolor-diagnostics \
-Wno-pre-c23-compat \
-Wno-declaration-after-statement \
-Wno-covered-switch-default"
)
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_C_FLAGS_DEBUG "-O0 -glldb")

find_library(foundation_framework CoreFoundation REQUIRED)
find_library(audio_framework CoreAudio REQUIRED)

add_executable(switchaudio
    main.c
    audio_switch.c
)

target_link_libraries(switchaudio PRIVATE
    ${foundation_framework}
    ${audio_framework}
)
