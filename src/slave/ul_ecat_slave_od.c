/**
 * @file ul_ecat_slave_od.c
 * @brief Object Dictionary lookup + read/write into entry storage.
 */

#include "ul_ecat_slave_od.h"

#include <string.h>

static const ul_ecat_od_table_t *g_table;

void ul_ecat_od_set_table(const ul_ecat_od_table_t *table)
{
    g_table = table;
}

const ul_ecat_od_entry_t *ul_ecat_od_lookup(uint16_t index, uint8_t subindex)
{
    if (g_table == NULL || g_table->entries == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < g_table->count; i++) {
        const ul_ecat_od_entry_t *e = &g_table->entries[i];
        if (e->index == index && e->subindex == subindex) {
            return e;
        }
    }
    return NULL;
}

const ul_ecat_od_entry_t *ul_ecat_od_first(void)
{
    if (g_table == NULL || g_table->entries == NULL || g_table->count == 0u) {
        return NULL;
    }
    return g_table->entries;
}

size_t ul_ecat_od_entries_count(void)
{
    return (g_table != NULL) ? g_table->count : 0u;
}

int ul_ecat_od_index_exists(uint16_t index)
{
    if (g_table == NULL || g_table->entries == NULL) {
        return 0;
    }
    for (size_t i = 0; i < g_table->count; i++) {
        if (g_table->entries[i].index == index) {
            return 1;
        }
    }
    return 0;
}

int ul_ecat_od_read(const ul_ecat_od_entry_t *entry, void *dst, size_t cap)
{
    if (entry == NULL || dst == NULL) {
        return UL_ECAT_OD_ERR_NOT_FOUND;
    }
    if ((entry->flags & UL_ECAT_OD_FLAG_R) == 0u) {
        return UL_ECAT_OD_ERR_NOT_READABLE;
    }
    if (entry->storage == NULL || entry->length == 0u) {
        return UL_ECAT_OD_ERR_LENGTH;
    }
    size_t n = entry->length;
    if (cap < n) {
        return UL_ECAT_OD_ERR_LENGTH;
    }
    memcpy(dst, entry->storage, n);
    return (int)n;
}

int ul_ecat_od_write(const ul_ecat_od_entry_t *entry, const void *src, size_t len)
{
    if (entry == NULL || src == NULL) {
        return UL_ECAT_OD_ERR_NOT_FOUND;
    }
    if ((entry->flags & UL_ECAT_OD_FLAG_W) == 0u) {
        return UL_ECAT_OD_ERR_NOT_WRITABLE;
    }
    if (entry->storage == NULL || entry->length == 0u) {
        return UL_ECAT_OD_ERR_LENGTH;
    }
    if (len > entry->length) {
        return UL_ECAT_OD_ERR_LENGTH;
    }
    memcpy(entry->storage, src, len);
    return UL_ECAT_OD_OK;
}
