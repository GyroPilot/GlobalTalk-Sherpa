# GlobalTalk Sherpa: build with Retro68 on moof (68000 code, so it runs on a Plus and up).
#   bash build.sh            (configures, builds, and refreshes the icons)
cmake_minimum_required(VERSION 3.10)
project(Sherpa C)

add_application(Sherpa
    src/main.c
    src/scan.c
    src/map.c
    src/store.c
    src/afp.c
    src/util.c
    src/autotest.c src/local.c
    src/sherpa.r
    TYPE "APPL"
    CREATOR "GTSh"
    )

target_include_directories(Sherpa PRIVATE src)
set_target_properties(Sherpa PROPERTIES COMPILE_OPTIONS "-ffunction-sections;-Os;-Wall;-Wno-multichar;-Wno-pointer-sign")
set_target_properties(Sherpa PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
