# LAN9252 — ports por SO

HAL SPI para integrar o driver em [`../`](..) (mesmo controlador) em RTOS:

| Diretório | Conteúdo |
|-----------|----------|
| [`zephyr/`](zephyr/) | HAL Zephyr (`/chosen ul-ecat-spi`, `SYS_INIT`, CS manual) |
| [`nuttx/`](nuttx/) | HAL NuttX (`SPI_LOCK` / `SPI_SELECT`, `lan9252_hal_nuttx_init`) |

O build Zephyr/NuttX aponta para estes caminhos via [`zephyr/CMakeLists.txt`](../../../zephyr/CMakeLists.txt), [`nuttx/Make.defs.slave_lan9252`](../../../nuttx/Make.defs.slave_lan9252) e [`nuttx/ul_ecat_slave_lan9252_sources.cmake`](../../../nuttx/ul_ecat_slave_lan9252_sources.cmake).
