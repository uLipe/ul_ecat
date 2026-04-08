/**
 * @file lan9252.c
 * @brief LAN9252 SPI driver — CSR / indirect ESC / PRAM access (see lan9252_regs.h).
 */

#include "lan9252.h"
#include "lan9252_hal.h"
#include "lan9252_regs.h"

#include <errno.h>
#include <string.h>

#define LAN9252_BUSY_MAX 100000u

int lan9252_init(void)
{
    return 0;
}

static int reg32_read(uint16_t addr, uint32_t *out);
static int reg32_write(uint16_t addr, uint32_t val);
static int csr_cmd_wait_not_busy(void);
static int pram_rd_wait_avail(void);
static int pram_wr_wait_avail(void);

static int reg32_read(uint16_t addr, uint32_t *out)
{
    uint8_t tx[4] = {
        LAN9252_SPI_FAST_READ,
        (uint8_t)((addr >> 8) & 0xFFu),
        (uint8_t)(addr & 0xFFu),
        (uint8_t)LAN9252_SPI_FAST_READ_DUMMY_BYTES,
    };
    uint8_t rx[4];

    if (lan9252_hal_spi_select() != 0) {
        return -EIO;
    }
    if (lan9252_hal_spi_write(tx, sizeof(tx)) != 0) {
        lan9252_hal_spi_deselect();
        return -EIO;
    }
    if (lan9252_hal_spi_read(rx, sizeof(rx)) != 0) {
        lan9252_hal_spi_deselect();
        return -EIO;
    }
    lan9252_hal_spi_deselect();
    *out = (uint32_t)rx[0] | ((uint32_t)rx[1] << 8) | ((uint32_t)rx[2] << 16) | ((uint32_t)rx[3] << 24);
    return 0;
}

static int reg32_write(uint16_t addr, uint32_t val)
{
    uint8_t tx[7] = {
        LAN9252_SPI_SERIAL_WRITE,
        (uint8_t)((addr >> 8) & 0xFFu),
        (uint8_t)(addr & 0xFFu),
        (uint8_t)(val & 0xFFu),
        (uint8_t)((val >> 8) & 0xFFu),
        (uint8_t)((val >> 16) & 0xFFu),
        (uint8_t)((val >> 24) & 0xFFu),
    };

    if (lan9252_hal_spi_select() != 0) {
        return -EIO;
    }
    if (lan9252_hal_spi_write(tx, sizeof(tx)) != 0) {
        lan9252_hal_spi_deselect();
        return -EIO;
    }
    lan9252_hal_spi_deselect();
    return 0;
}

static int csr_cmd_wait_not_busy(void)
{
    for (unsigned n = 0; n < LAN9252_BUSY_MAX; n++) {
        uint32_t v = 0;
        if (reg32_read(LAN9252_CSR_ESC_CSR_CMD, &v) != 0) {
            return -EIO;
        }
        if ((v & LAN9252_ESC_CSR_CMD_BUSY) == 0u) {
            return 0;
        }
    }
    return -ETIMEDOUT;
}

static int esc_read_csr_u16(uint16_t address, void *buf, uint16_t len)
{
    uint32_t cmd = LAN9252_ESC_CSR_CMD_READ | LAN9252_ESC_CSR_CMD_SIZE((uint32_t)len) | (uint32_t)address;
    if (reg32_write(LAN9252_CSR_ESC_CSR_CMD, cmd) != 0) {
        return -EIO;
    }
    if (csr_cmd_wait_not_busy() != 0) {
        return -ETIMEDOUT;
    }
    uint32_t data = 0;
    if (reg32_read(LAN9252_CSR_ESC_CSR_DATA, &data) != 0) {
        return -EIO;
    }
    memcpy(buf, &data, (size_t)len);
    return 0;
}

static int esc_write_csr_u16(uint16_t address, const void *buf, uint16_t len)
{
    uint32_t data = 0;
    memcpy(&data, buf, (size_t)len);
    if (reg32_write(LAN9252_CSR_ESC_CSR_DATA, data) != 0) {
        return -EIO;
    }
    uint32_t cmd = LAN9252_ESC_CSR_CMD_WRITE | LAN9252_ESC_CSR_CMD_SIZE((uint32_t)len) | (uint32_t)address;
    if (reg32_write(LAN9252_CSR_ESC_CSR_CMD, cmd) != 0) {
        return -EIO;
    }
    if (csr_cmd_wait_not_busy() != 0) {
        return -ETIMEDOUT;
    }
    return 0;
}

static uint16_t esc_next_chunk_size(uint16_t address, uint16_t len)
{
    uint16_t size = (len > 4u) ? 4u : len;
    if ((address & 1u) != 0u) {
        return 1u;
    }
    if ((address & 2u) != 0u) {
        return (uint16_t)((size & 1u) != 0u ? 1u : 2u);
    }
    if (size == 3u) {
        return 1u;
    }
    return size;
}

int lan9252_esc_read(uint16_t ado, void *buf, uint16_t len)
{
    uint8_t *p = (uint8_t *)buf;
    if (len == 0u) {
        return -EINVAL;
    }
    if (ado >= LAN9252_PDRAM_BASE) {
        return lan9252_pdram_read(ado, buf, len);
    }
    while (len > 0u) {
        uint16_t chunk = esc_next_chunk_size(ado, len);
        if (esc_read_csr_u16(ado, p, chunk) != 0) {
            return -EIO;
        }
        p += chunk;
        ado = (uint16_t)(ado + chunk);
        len = (uint16_t)(len - chunk);
    }
    return 0;
}

int lan9252_esc_write(uint16_t ado, const void *buf, uint16_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    if (len == 0u) {
        return -EINVAL;
    }
    if (ado >= LAN9252_PDRAM_BASE) {
        return lan9252_pdram_write(ado, buf, len);
    }
    while (len > 0u) {
        uint16_t chunk = esc_next_chunk_size(ado, len);
        if (esc_write_csr_u16(ado, p, chunk) != 0) {
            return -EIO;
        }
        p += chunk;
        ado = (uint16_t)(ado + chunk);
        len = (uint16_t)(len - chunk);
    }
    return 0;
}

static int pram_rd_wait_avail(void)
{
    for (unsigned n = 0; n < LAN9252_BUSY_MAX; n++) {
        uint32_t v = 0;
        if (reg32_read(LAN9252_CSR_PRAM_RD_CMD, &v) != 0) {
            return -EIO;
        }
        if ((v & LAN9252_PRAM_CMD_AVAIL) != 0u) {
            return 0;
        }
    }
    return -ETIMEDOUT;
}

static int pram_wr_wait_avail(void)
{
    for (unsigned n = 0; n < LAN9252_BUSY_MAX; n++) {
        uint32_t v = 0;
        if (reg32_read(LAN9252_CSR_PRAM_WR_CMD, &v) != 0) {
            return -EIO;
        }
        if ((v & LAN9252_PRAM_CMD_AVAIL) != 0u) {
            return 0;
        }
    }
    return -ETIMEDOUT;
}

static int pram_wait_rd_not_busy(void)
{
    for (unsigned n = 0; n < LAN9252_BUSY_MAX; n++) {
        uint32_t v = 0;
        if (reg32_read(LAN9252_CSR_PRAM_RD_CMD, &v) != 0) {
            return -EIO;
        }
        if ((v & LAN9252_PRAM_CMD_BUSY) == 0u) {
            return 0;
        }
    }
    return -ETIMEDOUT;
}

static int pram_wait_wr_not_busy(void)
{
    for (unsigned n = 0; n < LAN9252_BUSY_MAX; n++) {
        uint32_t v = 0;
        if (reg32_read(LAN9252_CSR_PRAM_WR_CMD, &v) != 0) {
            return -EIO;
        }
        if ((v & LAN9252_PRAM_CMD_BUSY) == 0u) {
            return 0;
        }
    }
    return -ETIMEDOUT;
}

int lan9252_pdram_read(uint16_t address, void *buf, uint16_t len)
{
    uint8_t *temp_buf = (uint8_t *)buf;
    uint16_t byte_offset = 0;

    if (len == 0u) {
        return -EINVAL;
    }

    if (reg32_write(LAN9252_CSR_PRAM_RD_CMD, LAN9252_PRAM_CMD_ABORT) != 0) {
        return -EIO;
    }
    if (pram_wait_rd_not_busy() != 0) {
        return -ETIMEDOUT;
    }

    uint32_t al = LAN9252_PRAM_SIZE((uint32_t)len) | LAN9252_PRAM_ADDR((uint32_t)address);
    if (reg32_write(LAN9252_CSR_PRAM_RD_ADDR_LEN, al) != 0) {
        return -EIO;
    }
    if (reg32_write(LAN9252_CSR_PRAM_RD_CMD, LAN9252_PRAM_CMD_BUSY) != 0) {
        return -EIO;
    }
    if (pram_rd_wait_avail() != 0) {
        return -ETIMEDOUT;
    }

    uint32_t v = 0;
    if (reg32_read(LAN9252_CSR_PRAM_RD_CMD, &v) != 0) {
        return -EIO;
    }
    uint8_t fifo_cnt = (uint8_t)LAN9252_PRAM_CMD_CNT(v);

    if (reg32_read(LAN9252_CSR_PRAM_RD_FIFO, &v) != 0) {
        return -EIO;
    }
    if (fifo_cnt > 0u) {
        fifo_cnt--;
    }

    uint8_t first_byte_position = (uint8_t)(address & 3u);
    uint16_t temp_len = (uint16_t)(((4u - (uint16_t)first_byte_position) > len)
                                       ? len
                                       : (4u - (uint16_t)first_byte_position));
    memcpy(temp_buf, ((const uint8_t *)&v) + first_byte_position, temp_len);
    len = (uint16_t)(len - temp_len);
    byte_offset = (uint16_t)(byte_offset + temp_len);

    if (len == 0u) {
        return 0;
    }

    uint8_t hdr[4] = {
        LAN9252_SPI_FAST_READ,
        (uint8_t)((LAN9252_CSR_PRAM_RD_FIFO >> 8) & 0xFFu),
        (uint8_t)(LAN9252_CSR_PRAM_RD_FIFO & 0xFFu),
        (uint8_t)LAN9252_SPI_FAST_READ_DUMMY_BYTES,
    };

    if (lan9252_hal_spi_select() != 0) {
        return -EIO;
    }
    if (lan9252_hal_spi_write(hdr, sizeof(hdr)) != 0) {
        lan9252_hal_spi_deselect();
        return -EIO;
    }

    while (len > 0u) {
        temp_len = (len > 4u) ? 4u : len;
        uint8_t chunk[4];
        if (lan9252_hal_spi_read(chunk, sizeof(chunk)) != 0) {
            lan9252_hal_spi_deselect();
            return -EIO;
        }
        memcpy(temp_buf + byte_offset, chunk, temp_len);
        if (fifo_cnt > 0u) {
            fifo_cnt--;
        }
        len = (uint16_t)(len - temp_len);
        byte_offset = (uint16_t)(byte_offset + temp_len);
    }
    lan9252_hal_spi_deselect();
    return 0;
}

int lan9252_pdram_write(uint16_t address, const void *buf, uint16_t len)
{
    const uint8_t *temp_buf = (const uint8_t *)buf;
    uint16_t byte_offset = 0;

    if (len == 0u) {
        return -EINVAL;
    }

    if (reg32_write(LAN9252_CSR_PRAM_WR_CMD, LAN9252_PRAM_CMD_ABORT) != 0) {
        return -EIO;
    }
    if (pram_wait_wr_not_busy() != 0) {
        return -ETIMEDOUT;
    }

    uint32_t al = LAN9252_PRAM_SIZE((uint32_t)len) | LAN9252_PRAM_ADDR((uint32_t)address);
    if (reg32_write(LAN9252_CSR_PRAM_WR_ADDR_LEN, al) != 0) {
        return -EIO;
    }
    if (reg32_write(LAN9252_CSR_PRAM_WR_CMD, LAN9252_PRAM_CMD_BUSY) != 0) {
        return -EIO;
    }
    if (pram_wr_wait_avail() != 0) {
        return -ETIMEDOUT;
    }

    uint32_t v = 0;
    if (reg32_read(LAN9252_CSR_PRAM_WR_CMD, &v) != 0) {
        return -EIO;
    }
    uint8_t fifo_cnt = (uint8_t)LAN9252_PRAM_CMD_CNT(v);

    uint8_t first_byte_position = (uint8_t)(address & 3u);
    uint16_t temp_len = (uint16_t)(((4u - (uint16_t)first_byte_position) > len)
                                       ? len
                                       : (4u - (uint16_t)first_byte_position));

    uint32_t word = 0;
    memcpy(((uint8_t *)&word) + first_byte_position, temp_buf, temp_len);
    if (reg32_write(LAN9252_CSR_PRAM_WR_FIFO, word) != 0) {
        return -EIO;
    }
    if (fifo_cnt > 0u) {
        fifo_cnt--;
    }

    len = (uint16_t)(len - temp_len);
    byte_offset = (uint16_t)(byte_offset + temp_len);

    if (len == 0u) {
        return 0;
    }

    uint8_t hdr[3] = {
        LAN9252_SPI_SERIAL_WRITE,
        (uint8_t)((LAN9252_CSR_PRAM_WR_FIFO >> 8) & 0xFFu),
        (uint8_t)(LAN9252_CSR_PRAM_WR_FIFO & 0xFFu),
    };

    if (lan9252_hal_spi_select() != 0) {
        return -EIO;
    }
    if (lan9252_hal_spi_write(hdr, sizeof(hdr)) != 0) {
        lan9252_hal_spi_deselect();
        return -EIO;
    }

    while (len > 0u) {
        temp_len = (len > 4u) ? 4u : len;
        uint32_t w = 0;
        memcpy(&w, temp_buf + byte_offset, temp_len);
        if (lan9252_hal_spi_write((const uint8_t *)&w, sizeof(w)) != 0) {
            lan9252_hal_spi_deselect();
            return -EIO;
        }
        if (fifo_cnt > 0u) {
            fifo_cnt--;
        }
        len = (uint16_t)(len - temp_len);
        byte_offset = (uint16_t)(byte_offset + temp_len);
    }
    lan9252_hal_spi_deselect();
    return 0;
}

int lan9252_read_csr_u32(uint16_t csr_addr, uint32_t *out)
{
    if (out == NULL) {
        return -EINVAL;
    }
    return reg32_read(csr_addr, out);
}

int lan9252_write_csr_u32(uint16_t csr_addr, uint32_t val)
{
    return reg32_write(csr_addr, val);
}

int lan9252_soft_reset(void)
{
    if (reg32_write(LAN9252_CSR_RESET_CTRL, LAN9252_RESET_CTRL_RST) != 0) {
        return -EIO;
    }
    for (unsigned n = 0; n < LAN9252_BUSY_MAX; n++) {
        uint32_t r = 0;
        if (reg32_read(LAN9252_CSR_RESET_CTRL, &r) != 0) {
            return -EIO;
        }
        if ((r & LAN9252_RESET_CTRL_RST) == 0u) {
            return 0;
        }
    }
    return -ETIMEDOUT;
}
