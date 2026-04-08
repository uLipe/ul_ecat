# LAN9252 SPI driver (minimal)

Portable C99 helpers for the **Microchip LAN9252** EtherCAT Slave Controller accessed over the **SPI PDI** (serial mode). The chip terminates the EtherCAT network; the MCU only moves data over SPI.

## References

- Register map and SPI sequences are aligned with the **SOES** reference (`soes/hal/rt-kernel-lan9252/esc_hw.c`, OpenEtherCAT Society) and the Microchip LAN9252 datasheet.
- See [`doc/lan9252-datasheet-reference.md`](../../doc/lan9252-datasheet-reference.md) for the official documentation link.

## API overview

| Function | Purpose |
|----------|---------|
| `lan9252_init` | No-op placeholder (extend for chip ID checks). |
| `lan9252_soft_reset` | Pulse `RESET_CTRL` (`0x1F8`) and poll until clear. |
| `lan9252_read_csr_u32` / `lan9252_write_csr_u32` | Direct 32-bit access to internal CSR addresses via SPI. |
| `lan9252_esc_read` / `lan9252_esc_write` | EtherCAT core registers **ADO &lt; 0x1000** via indirect CSR (`0x300`/`0x304`). |
| `lan9252_pdram_read` / `lan9252_pdram_write` | Process RAM (**ADO ≥ 0x1000**) via PRAM engines and FIFOs (`0x000`/`0x020`, commands `0x308`–`0x314`). |

## HAL (board / RTOS)

Implement **strong** symbols for:

- `lan9252_hal_spi_select` / `lan9252_hal_spi_deselect`
- `lan9252_hal_spi_write`
- `lan9252_hal_spi_read` (MOSI idle low; clock out zeros while reading MISO)

Default **weak** stubs in `lan9252_hal_weak.c` return errors so CI can link without hardware.

SPI framing matches SOES: **serial write** `0x02` + 16-bit address + data; **fast read** `0x0B` + address + 1 dummy + data.

## Real hardware notes

- A valid **EEPROM / SII** image is usually required before the ESC behaves as expected on a custom board.
- Microchip examples often use **SPI indirect** PDI (`EEPROM` config `0x80` in their SSC XML). Match your hardware strapping.

## CMake

```cmake
set(UL_ECAT_BUILD_LAN9252 ON CACHE BOOL "")
# after find_package(ul_ecat) or add_subdirectory(ul_ecat)
target_link_libraries(my_firmware PRIVATE ul_ecat::lan9252)
```

Replace `lan9252_hal_weak.c` in your target with your own `lan9252_hal.c` (or exclude the weak file from the build and supply implementations).

## Integration with `ul_ecat_slave_controller`

When both `UL_ECAT_BUILD_SLAVE` and `UL_ECAT_BUILD_LAN9252` are enabled, `libul_ecat_slave` includes the LAN9252 poll backend. Use [`ul_ecat_slave_controller.h`](../../include/ul_ecat_slave_controller.h) with backend `UL_ECAT_SLAVE_BACKEND_LAN9252_SPI`, implement the SPI HAL, and call `ul_ecat_slave_controller_poll` in your main loop. Datagram TX/RX on the wire are handled inside the LAN9252; this driver only updates the ESC register mirror and optional PDO buffers over SPI.
