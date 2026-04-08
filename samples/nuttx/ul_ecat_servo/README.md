# NuttX: EtherCAT slave + LAN9252 (servo PDO sample)

## Kconfig

In `apps/Kconfig`, source:

- [`nuttx/Kconfig`](../../../nuttx/Kconfig)
- [`samples/nuttx/ul_ecat_servo/Kconfig`](Kconfig)

Enable `CONFIG_UL_ECAT`, `CONFIG_UL_ECAT_SLAVE`, `CONFIG_SPI`, your board’s SPI driver (SPI0), and `CONFIG_EXAMPLES_UL_ECAT_SERVO`. See [`defconfig.snippet`](defconfig.snippet).

## SPI0

Implement **`board_ul_ecat_lan9252_spi0()`** in your board support or application (strong symbol), returning a pointer to the NuttX `struct spi_dev_s *` for **SPI0** (chip-select index `0` is passed to `lan9252_hal_nuttx_init`). The weak default in `ul_ecat_servo_main.c` returns `NULL` and prints a message.

Example pattern (STM32, illustrative only):

```c
#include <nuttx/spi/spi.h>
struct spi_dev_s *stm32_spibus_initialize(int port);

struct spi_dev_s *board_ul_ecat_lan9252_spi0(void)
{
	return stm32_spibus_initialize(0);
}
```

## Build

Install this directory under `apps/examples/ul_ecat_servo` or set `UL_ECAT_ROOT` in the Makefile (default resolves three levels up to the `ul_ecat` repo root).
