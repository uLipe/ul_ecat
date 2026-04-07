# Copyright (c) ul_ecat contributors
# SPDX-License-Identifier: MIT
#
# Usage (from a NuttX or app CMakeLists.txt):
#   set(UL_ECAT_ROOT "/path/to/ul_ecat" CACHE PATH "ul_ecat repository root")
#   include(${UL_ECAT_ROOT}/nuttx/ul_ecat_sources.cmake)
#   target_sources(... PRIVATE ${UL_ECAT_NUTTX_SOURCES})
#   target_include_directories(... PRIVATE ${UL_ECAT_NUTTX_INCLUDE_DIR})

set(UL_ECAT_NUTTX_INCLUDE_DIR "${UL_ECAT_ROOT}/include")

set(UL_ECAT_NUTTX_SOURCES
    "${UL_ECAT_ROOT}/src/ul_ecat_frame.c"
    "${UL_ECAT_ROOT}/src/ul_ecat_al.c"
    "${UL_ECAT_ROOT}/src/ul_ecat_master.c"
    "${UL_ECAT_ROOT}/src/osal/osal_nuttx.c"
    "${UL_ECAT_ROOT}/src/transport/transport_nuttx.c"
)
