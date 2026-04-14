# SPDX-License-Identifier: MIT
"""
Shared EtherCAT wire-level constants for Python scripts and tests.

Values mirror the C headers (ul_ecat_frame.h, ul_ecat_esc_regs.h) and the
Beckhoff EL7201-like identity used in all simulators and test expectations.
"""

ETH_P_ECAT = 0x88A4

# Datagram command bytes (ETG.1000 Table 11)
UL_ECAT_CMD_APRD = 0x01
UL_ECAT_CMD_APWR = 0x02
UL_ECAT_CMD_FPRD = 0x04
UL_ECAT_CMD_FPWR = 0x05
UL_ECAT_CMD_BRD  = 0x07
UL_ECAT_CMD_BWR  = 0x08
UL_ECAT_CMD_LRD  = 0x0A
UL_ECAT_CMD_LWR  = 0x0B
UL_ECAT_CMD_LRW  = 0x0C

# Datagram header / WKC sizes
UL_ECAT_DGRAM_HDR_LEN = 10
UL_ECAT_DGRAM_WKC_LEN = 2

# ESC register offsets (byte-addressed, match ul_ecat_esc_regs.h)
ESC_REG_STADR   = 0x0010
ESC_REG_VENDOR  = 0x0012
ESC_REG_PRODUCT = 0x0016
ESC_REG_REV     = 0x001A
ESC_REG_SERIAL  = 0x001E
ESC_REG_ALCTL   = 0x0120
ESC_REG_ALSTAT    = 0x0130
ESC_REG_ALSTACODE = 0x0134
ESC_REG_ALEVENT   = 0x0220
ESC_REG_DCSYS0  = 0x0910
ESC_REG_DCSYSOFS = 0x0920

# AL Status Code values (ETG.1000 Table 11, subset)
AL_ERR_NONE                    = 0x0000
AL_ERR_INVALID_STATE_CHANGE    = 0x0011
AL_ERR_UNKNOWN_STATE           = 0x0012
AL_ERR_BOOTSTRAP_NOT_SUPPORTED = 0x0013

# AL state values (nibble)
AL_STATE_INIT   = 1
AL_STATE_PREOP  = 2
AL_STATE_BOOT   = 3
AL_STATE_SAFEOP = 4
AL_STATE_OP     = 8

# Valid transitions: set of (from_state, to_state)
AL_VALID_TRANSITIONS = frozenset({
    (AL_STATE_INIT,   AL_STATE_PREOP),
    (AL_STATE_PREOP,  AL_STATE_SAFEOP),
    (AL_STATE_SAFEOP, AL_STATE_OP),
    (AL_STATE_OP,     AL_STATE_SAFEOP),
    (AL_STATE_SAFEOP, AL_STATE_PREOP),
    (AL_STATE_PREOP,  AL_STATE_INIT),
    (AL_STATE_SAFEOP, AL_STATE_INIT),
    (AL_STATE_OP,     AL_STATE_INIT),
})

# Default identity (Beckhoff EL7201-like, public product data)
DEFAULT_VENDOR   = 0x00000002
DEFAULT_PRODUCT  = 0x1C213052
DEFAULT_REVISION = 0x00000001
DEFAULT_SERIAL   = 0x00000001
