/**
 * @file lan9252_hal_weak.c
 * @brief Default weak HAL stubs — replace with real SPI in your BSP / RTOS port.
 */

#include "lan9252_hal.h"

#include <errno.h>

#if defined(__GNUC__) || defined(__clang__)
#define LAN9252_WEAK __attribute__((weak))
#else
#define LAN9252_WEAK
#endif

LAN9252_WEAK int lan9252_hal_spi_select(void)
{
    (void)0;
    return -ENODEV;
}

LAN9252_WEAK void lan9252_hal_spi_deselect(void) {}

LAN9252_WEAK int lan9252_hal_spi_write(const uint8_t *data, size_t len)
{
    (void)data;
    (void)len;
    return -ENODEV;
}

LAN9252_WEAK int lan9252_hal_spi_read(uint8_t *data, size_t len)
{
    (void)data;
    (void)len;
    return -ENODEV;
}

LAN9252_WEAK void lan9252_hal_delay_us(uint32_t us)
{
    (void)us;
}
