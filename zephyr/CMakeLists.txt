# Copyright (c) ul_ecat contributors
# SPDX-License-Identifier: MIT

if(CONFIG_UL_ECAT)
  zephyr_library()

  set(UL_ECAT_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

  zephyr_include_directories(${UL_ECAT_ROOT}/include)

  zephyr_library_sources(
    ${UL_ECAT_ROOT}/src/ul_ecat_frame.c
    ${UL_ECAT_ROOT}/src/ul_ecat_al.c
    ${UL_ECAT_ROOT}/src/ul_ecat_master.c
    ${UL_ECAT_ROOT}/src/osal/osal_zephyr.c
  )

  if(CONFIG_UL_ECAT_TRANSPORT_NETDEV)
    zephyr_library_sources(${UL_ECAT_ROOT}/src/transport/transport_zephyr_netdev.c)
  else()
    zephyr_library_sources(${UL_ECAT_ROOT}/src/transport/transport_zephyr.c)
  endif()
endif()
