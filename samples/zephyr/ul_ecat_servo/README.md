# Zephyr: EtherCAT slave + LAN9252 (servo PDO sample)

Requires a board with SPI and a devicetree overlay that:

1. Sets `/chosen` property `ul-ecat-spi` to the SPI **device** node for the LAN9252 (child of the SPI bus), matching [`ports/lan9252/zephyr/hal_zephyr.c`](../../../ports/lan9252/zephyr/hal_zephyr.c).
2. Defines `cs-gpios` on the SPI controller (often in the board DTS) and a child node with `compatible = "vnd,spi-device"`, `reg = <0>`, and `spi-max-frequency`.

CI compiles against **ESP32-DevKitC (WROOM)** — see [`boards/esp32_devkitc_wroom_procpu.overlay`](boards/esp32_devkitc_wroom_procpu.overlay) (`SPI2`). For other boards, add `boards/<board>_<soc>.overlay` with your `&spiN` and CS GPIO.

Build (from this directory, with Zephyr environment):

```bash
export ZEPHYR_EXTRA_MODULES=/absolute/path/to/ul_ecat
west build -b <board> -p always .
```

Enable `CONFIG_SPI` and `CONFIG_GPIO` in `prj.conf` (already set). The HAL is selected with `CONFIG_UL_ECAT_LAN9252_HAL=y`.

**Note:** `native_sim` may not provide a suitable SPI devicetree; use a hardware board or a QEMU target with SPI in DTS.
