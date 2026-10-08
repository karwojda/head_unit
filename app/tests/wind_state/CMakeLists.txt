cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wind_state_test)

target_sources(app PRIVATE src/main.c src/live_wind.c)
target_include_directories(app PRIVATE ../../src)
target_sources(app PRIVATE ../../src/wind_state.c ../../src/ess_wind.c)
