# LAN9252 EtherCAT slave controller — SPI bridge (no link to libul_ecat_slave by default).

add_library(ul_ecat_lan9252 STATIC
    src/lan9252.c
    src/lan9252_hal_weak.c
)
add_library(ul_ecat::lan9252 ALIAS ul_ecat_lan9252)

target_include_directories(ul_ecat_lan9252 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(ul_ecat_lan9252 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

# Install/export is done from the root CMakeLists (ul_ecatTargets).
