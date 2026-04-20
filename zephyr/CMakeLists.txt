# Copyright (c) ul_ecat contributors
# SPDX-License-Identifier: MIT

if(CONFIG_UL_ECAT)
  if(NOT CONFIG_UL_ECAT_MASTER AND NOT CONFIG_UL_ECAT_SLAVE)
    message(FATAL_ERROR "ul_ecat: enable CONFIG_UL_ECAT_MASTER and/or CONFIG_UL_ECAT_SLAVE")
  endif()

  zephyr_library()

  set(UL_ECAT_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

  zephyr_include_directories(${UL_ECAT_ROOT}/include)

  zephyr_library_sources(${UL_ECAT_ROOT}/src/common/ul_ecat_frame.c)
  zephyr_library_sources(${UL_ECAT_ROOT}/src/common/ul_ecat_al.c)

  if(CONFIG_UL_ECAT_MASTER)
    zephyr_library_sources(${UL_ECAT_ROOT}/src/master/ul_ecat_master.c)
    zephyr_library_sources(${UL_ECAT_ROOT}/src/osal/osal_zephyr.c)
    if(CONFIG_UL_ECAT_TRANSPORT_NETDEV)
      zephyr_library_sources(${UL_ECAT_ROOT}/src/transport/transport_zephyr_netdev.c)
    else()
      zephyr_library_sources(${UL_ECAT_ROOT}/src/transport/transport_zephyr.c)
    endif()
  endif()

  if(CONFIG_UL_ECAT_SLAVE)
    zephyr_include_directories(${UL_ECAT_ROOT}/generated)
    zephyr_include_directories(${UL_ECAT_ROOT}/src/slave)
    zephyr_include_directories(${UL_ECAT_ROOT}/controllers/lan9252/include)
    zephyr_include_directories(${UL_ECAT_ROOT}/controllers/lan9252/ports/zephyr)

    zephyr_library_sources(${UL_ECAT_ROOT}/src/slave/ul_ecat_esc.c)
    zephyr_library_sources(${UL_ECAT_ROOT}/src/slave/ul_ecat_slave_al.c)
    zephyr_library_sources(${UL_ECAT_ROOT}/src/slave/ul_ecat_slave_mailbox.c)
    zephyr_library_sources(${UL_ECAT_ROOT}/src/slave/ul_ecat_slave_od.c)
    zephyr_library_sources(${UL_ECAT_ROOT}/src/slave/ul_ecat_slave_pdu.c)
    zephyr_library_sources(${UL_ECAT_ROOT}/src/slave/ul_ecat_slave.c)
    zephyr_library_sources(${UL_ECAT_ROOT}/src/slave/ul_ecat_slave_controller.c)
    zephyr_library_sources(${UL_ECAT_ROOT}/src/slave/ul_ecat_slave_controller_lan9252.c)
    zephyr_library_sources(${UL_ECAT_ROOT}/generated/ul_ecat_slave_tables.c)
    zephyr_library_sources(${UL_ECAT_ROOT}/controllers/lan9252/src/lan9252.c)

    zephyr_compile_definitions(UL_ECAT_HAVE_LAN9252_CONTROLLER=1)

    if(CONFIG_UL_ECAT_LAN9252_HAL)
      zephyr_library_sources(${UL_ECAT_ROOT}/controllers/lan9252/ports/zephyr/hal_zephyr.c)
    else()
      zephyr_library_sources(${UL_ECAT_ROOT}/controllers/lan9252/src/lan9252_hal_weak.c)
    endif()
  endif()
endif()
