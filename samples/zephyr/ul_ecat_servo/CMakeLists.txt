# SPDX-License-Identifier: MIT
#
# EtherCAT slave + LAN9252 HAL (Zephyr). Requires a board devicetree with SPI
# and app.overlay setting /chosen ul-ecat-spi. See README in this directory.
#
cmake_minimum_required(VERSION 3.20)

set(UL_ECAT_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
list(APPEND ZEPHYR_EXTRA_MODULES ${UL_ECAT_REPO_ROOT})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ul_ecat_servo)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../common)
target_sources(app PRIVATE src/main.c)
