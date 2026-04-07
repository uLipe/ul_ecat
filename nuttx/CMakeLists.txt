# Copyright (c) ul_ecat contributors
# SPDX-License-Identifier: MIT
#
# Optional CMake helper for NuttX out-of-tree integration.
# Set UL_ECAT_ROOT to the ul_ecat repository root before add_subdirectory or include.
#
# Example:
#   set(UL_ECAT_ROOT "/path/to/ul_ecat")
#   include(${UL_ECAT_ROOT}/nuttx/ul_ecat_sources.cmake)
#   target_sources(myapp PRIVATE ${UL_ECAT_NUTTX_SOURCES})
#   target_include_directories(myapp PRIVATE ${UL_ECAT_NUTTX_INCLUDE_DIR})

if(NOT UL_ECAT_ROOT)
  message(FATAL_ERROR "UL_ECAT_ROOT must be set to the ul_ecat repository root")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/ul_ecat_sources.cmake)

# Convenience interface library (optional; parent may use variables only)
if(NOT TARGET ul_ecat_nuttx)
  add_library(ul_ecat_nuttx INTERFACE)
  target_sources(ul_ecat_nuttx INTERFACE ${UL_ECAT_NUTTX_SOURCES})
  target_include_directories(ul_ecat_nuttx INTERFACE ${UL_ECAT_NUTTX_INCLUDE_DIR})
endif()
