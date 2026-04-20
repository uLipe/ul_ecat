# Copyright (c) ul_ecat contributors
# SPDX-License-Identifier: MIT
#
# NuttX: EtherCAT slave + LAN9252 + hal_nuttx (SPI). Include after ul_ecat_sources.cmake
# or standalone with UL_ECAT_ROOT set.
#
#   include(${UL_ECAT_ROOT}/nuttx/ul_ecat_slave_lan9252_sources.cmake)
#   target_sources(myapp PRIVATE ${UL_ECAT_NUTTX_SLAVE_LAN9252_SOURCES})
#   target_include_directories(myapp PRIVATE ${UL_ECAT_NUTTX_SLAVE_LAN9252_INCLUDE_DIRS})
#   target_compile_definitions(myapp PRIVATE UL_ECAT_HAVE_LAN9252_CONTROLLER=1)

if(NOT UL_ECAT_ROOT)
  message(FATAL_ERROR "UL_ECAT_ROOT must be set to the ul_ecat repository root")
endif()

set(UL_ECAT_NUTTX_SLAVE_LAN9252_SOURCES
    "${UL_ECAT_ROOT}/src/common/ul_ecat_frame.c"
    "${UL_ECAT_ROOT}/src/common/ul_ecat_al.c"
    "${UL_ECAT_ROOT}/src/slave/ul_ecat_esc.c"
    "${UL_ECAT_ROOT}/src/slave/ul_ecat_slave_al.c"
    "${UL_ECAT_ROOT}/src/slave/ul_ecat_slave_coe.c"
    "${UL_ECAT_ROOT}/src/slave/ul_ecat_slave_mailbox.c"
    "${UL_ECAT_ROOT}/src/slave/ul_ecat_slave_od.c"
    "${UL_ECAT_ROOT}/src/slave/ul_ecat_slave_pdu.c"
    "${UL_ECAT_ROOT}/src/slave/ul_ecat_slave.c"
    "${UL_ECAT_ROOT}/src/slave/ul_ecat_slave_controller.c"
    "${UL_ECAT_ROOT}/src/slave/ul_ecat_slave_controller_lan9252.c"
    "${UL_ECAT_ROOT}/generated/ul_ecat_slave_tables.c"
    "${UL_ECAT_ROOT}/controllers/lan9252/src/lan9252.c"
    "${UL_ECAT_ROOT}/controllers/lan9252/ports/nuttx/hal_nuttx.c"
)

set(UL_ECAT_NUTTX_SLAVE_LAN9252_INCLUDE_DIRS
    "${UL_ECAT_ROOT}/include"
    "${UL_ECAT_ROOT}/generated"
    "${UL_ECAT_ROOT}/src"
    "${UL_ECAT_ROOT}/src/slave"
    "${UL_ECAT_ROOT}/controllers/lan9252/include"
    "${UL_ECAT_ROOT}/controllers/lan9252/ports/nuttx"
)
