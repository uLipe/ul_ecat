# SPDX-License-Identifier: MIT
#
# Build from this directory with the ul_ecat repo registered as a Zephyr module, e.g.:
#   export ZEPHYR_EXTRA_MODULES=/path/to/ul_ecat
#   west build -b native_sim/native/64 -- -DZEPHYR_EXTRA_MODULES=/path/to/ul_ecat
#
# Or prepend the repo root to ZEPHYR_EXTRA_MODULES from this sample (three levels up):
cmake_minimum_required(VERSION 3.20)

set(UL_ECAT_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
list(APPEND ZEPHYR_EXTRA_MODULES ${UL_ECAT_REPO_ROOT})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ul_ecat_scan)

target_sources(app PRIVATE src/main.c)
