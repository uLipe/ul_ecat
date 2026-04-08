# RTOS integration: LAN9252 HAL (Zephyr and NuttX)

This document describes how the EtherCAT **slave** stack talks to the LAN9252 over SPI on Zephyr and NuttX. A **Linux** SPI HAL for the slave is intentionally **not** part of this repository for now.

## Zephyr

- **Kconfig:** `CONFIG_UL_ECAT_SLAVE` builds the ESC mirror, PDU layer, `ul_ecat_slave_controller`, and `lan9252.c`. `CONFIG_UL_ECAT_LAN9252_HAL` (default on when slave is enabled) builds [`ports/lan9252/zephyr/hal_zephyr.c`](../ports/lan9252/zephyr/hal_zephyr.c) and **selects `CONFIG_SPI`**.
- **Devicetree:** Set **`/chosen` `ul-ecat-spi`** to the SPI **device** node (child of the SPI controller) for the LAN9252. The HAL runs a **`SYS_INIT`** at `POST_KERNEL` / `CONFIG_APPLICATION_INIT_PRIORITY` that calls `SPI_DT_SPEC_GET` and configures **manual CS** (GPIO), because the LAN9252 driver performs separate `write` / `read` calls that must keep CS low across both.
- **Sample:** [`samples/zephyr/ul_ecat_servo`](../samples/zephyr/ul_ecat_servo/) — see `boards/*.overlay` and `README.md`.

## NuttX

- **Kconfig:** `CONFIG_UL_ECAT_SLAVE` depends on `UL_ECAT` and **selects SPI**. Enable your board’s SPI driver (e.g. SPI0) in defconfig; see [`samples/nuttx/ul_ecat_servo/defconfig.snippet`](../samples/nuttx/ul_ecat_servo/defconfig.snippet).
- **HAL:** [`ports/lan9252/nuttx/hal_nuttx.c`](../ports/lan9252/nuttx/hal_nuttx.c) implements `lan9252_hal_*` using `SPI_LOCK` / `SPI_SELECT` / `SPI_SNDBLOCK` / `SPI_RECVBLOCK`. Call **`lan9252_hal_nuttx_init(spi, devid)`** before `lan9252_init()` (typically **SPI0**, `devid == 0`).
- **Makefile / CMake:** [`nuttx/Make.defs.slave_lan9252`](../nuttx/Make.defs.slave_lan9252) and [`nuttx/ul_ecat_slave_lan9252_sources.cmake`](../nuttx/ul_ecat_slave_lan9252_sources.cmake) list sources under `ports/lan9252/nuttx/` and `UL_ECAT_HAVE_LAN9252_CONTROLLER=1`.
- **Sample:** [`samples/nuttx/ul_ecat_servo`](../samples/nuttx/ul_ecat_servo/) implements **`board_ul_ecat_lan9252_spi0()`** as a weak stub; override it to return your SPI0 `struct spi_dev_s *`.

## Servo PDO layout (samples)

Shared header [`samples/common/ul_ecat_servo_sample.h`](../samples/common/ul_ecat_servo_sample.h) defines a **fictitious** identity and fixed PDO offsets `0x1000` / `0x1100` (12-byte int32 position, velocity, torque). This is not a full CoE/ESI mapping; it is only a demonstration convention for process data exchanged via `ul_ecat_slave_controller_set_pdram`.
