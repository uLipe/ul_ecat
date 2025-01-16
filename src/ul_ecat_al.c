/**
 * @file ul_ecat_al.c
 * @brief AL Control / Status helpers.
 */

#include "ul_ecat_al.h"

uint16_t ul_ecat_al_control_word(uint8_t state_nibble, int ack)
{
    uint16_t w = (uint16_t)((uint16_t)state_nibble & UL_ECAT_AL_MASK_STATE);
    if (ack) {
        w = (uint16_t)(w | UL_ECAT_AL_CTRL_ACK);
    }
    return w;
}

uint8_t ul_ecat_al_status_state(uint16_t al_status)
{
    return (uint8_t)(al_status & UL_ECAT_AL_MASK_STATE);
}

int ul_ecat_al_status_error_indicated(uint16_t al_status)
{
    return (al_status & UL_ECAT_AL_STAT_ERR) != 0 ? 1 : 0;
}
