/**
 * @file lan9252_full_mock.c
 * @brief In-memory LAN9252 API for unit tests (evidence chain: controller -> expected register writes).
 */

#include "lan9252_full_mock.h"
#include "lan9252.h"

#include <errno.h>
#include <string.h>

#define MOCK_ESC_BYTES 4096u
#define MOCK_PDRAM_BYTES 8192u
#define MOCK_CSR_BYTES 0x400u

static uint8_t g_esc_core[MOCK_ESC_BYTES];
static uint8_t g_pdram[MOCK_PDRAM_BYTES];
static uint8_t g_csr[MOCK_CSR_BYTES];

uint8_t *lan9252_mock_esc_core(void)
{
    return g_esc_core;
}

uint8_t *lan9252_mock_pdram(void)
{
    return g_pdram;
}

uint8_t *lan9252_mock_csr_bytes(void)
{
    return g_csr;
}

void lan9252_mock_reset(void)
{
    memset(g_esc_core, 0, sizeof(g_esc_core));
    memset(g_pdram, 0, sizeof(g_pdram));
    memset(g_csr, 0, sizeof(g_csr));
}

int lan9252_init(void)
{
    return 0;
}

int lan9252_soft_reset(void)
{
    return 0;
}

int lan9252_read_csr_u32(uint16_t csr_addr, uint32_t *out)
{
    if (out == NULL || (size_t)csr_addr + 4u > MOCK_CSR_BYTES) {
        return -EINVAL;
    }
    *out = (uint32_t)g_csr[csr_addr] | ((uint32_t)g_csr[csr_addr + 1] << 8) | ((uint32_t)g_csr[csr_addr + 2] << 16) |
           ((uint32_t)g_csr[csr_addr + 3] << 24);
    return 0;
}

int lan9252_write_csr_u32(uint16_t csr_addr, uint32_t val)
{
    if ((size_t)csr_addr + 4u > MOCK_CSR_BYTES) {
        return -EINVAL;
    }
    g_csr[csr_addr] = (uint8_t)(val & 0xFFu);
    g_csr[csr_addr + 1] = (uint8_t)((val >> 8) & 0xFFu);
    g_csr[csr_addr + 2] = (uint8_t)((val >> 16) & 0xFFu);
    g_csr[csr_addr + 3] = (uint8_t)((val >> 24) & 0xFFu);
    return 0;
}

int lan9252_esc_read(uint16_t ado, void *buf, uint16_t len)
{
    if (buf == NULL || len == 0u) {
        return -EINVAL;
    }
    if ((uint32_t)ado + (uint32_t)len > MOCK_ESC_BYTES) {
        return -EINVAL;
    }
    memcpy(buf, g_esc_core + ado, len);
    return 0;
}

int lan9252_esc_write(uint16_t ado, const void *buf, uint16_t len)
{
    if (buf == NULL || len == 0u) {
        return -EINVAL;
    }
    if ((uint32_t)ado + (uint32_t)len > MOCK_ESC_BYTES) {
        return -EINVAL;
    }
    memcpy(g_esc_core + ado, buf, len);
    return 0;
}

int lan9252_pdram_read(uint16_t offset, void *buf, uint16_t len)
{
    if (buf == NULL || len == 0u) {
        return -EINVAL;
    }
    if (offset < 0x1000u) {
        return -EINVAL;
    }
    size_t idx = (size_t)offset - 0x1000u;
    if (idx + (size_t)len > MOCK_PDRAM_BYTES) {
        return -EINVAL;
    }
    memcpy(buf, g_pdram + idx, len);
    return 0;
}

int lan9252_pdram_write(uint16_t offset, const void *buf, uint16_t len)
{
    if (buf == NULL || len == 0u) {
        return -EINVAL;
    }
    if (offset < 0x1000u) {
        return -EINVAL;
    }
    size_t idx = (size_t)offset - 0x1000u;
    if (idx + (size_t)len > MOCK_PDRAM_BYTES) {
        return -EINVAL;
    }
    memcpy(g_pdram + idx, buf, len);
    return 0;
}
