# SPDX-License-Identifier: MIT
"""
Shared EtherCAT frame/datagram encode/parse for Python scripts and tests.

Wire layout matches src/common/ul_ecat_frame.c (little-endian, ETG.1000).
"""
from __future__ import annotations

import struct
from typing import List, Optional, Tuple

from ecat_wire import ETH_P_ECAT, UL_ECAT_DGRAM_HDR_LEN, UL_ECAT_DGRAM_WKC_LEN


def w16_le(v: int) -> bytes:
    return bytes([v & 0xFF, (v >> 8) & 0xFF])


def r16_le(b: bytes, off: int) -> int:
    return b[off] | (b[off + 1] << 8)


def w32_le(v: int) -> bytes:
    return struct.pack("<I", v & 0xFFFFFFFF)


def r32_le(b: bytes, off: int = 0) -> int:
    return struct.unpack_from("<I", b, off)[0]


def dgram_encode(
    cmd: int,
    index: int,
    adp: int,
    ado: int,
    data_len: int,
    irq: int,
    wkc: int,
    data: Optional[bytes] = None,
) -> bytes:
    out = bytearray()
    out.append(cmd & 0xFF)
    out.append(index & 0xFF)
    out.extend(w16_le(adp))
    out.extend(w16_le(ado))
    out.extend(w16_le(data_len & 0x7FF))
    out.extend(w16_le(irq))
    if data_len > 0:
        if data is not None:
            out.extend(data[:data_len].ljust(data_len, b"\0"))
        else:
            out.extend(b"\0" * data_len)
    out.extend(w16_le(wkc))
    return bytes(out)


def dgram_repack(
    cmd: int, idx: int, adp: int, ado: int, dlen: int, irq: int,
    data: bytearray, wkc: int,
) -> bytes:
    """Re-encode a datagram from parsed fields (response path)."""
    out = bytearray()
    out.append(cmd)
    out.append(idx & 0xFF)
    out.extend(w16_le(adp))
    out.extend(w16_le(ado))
    out.extend(w16_le(dlen & 0x7FF))
    out.extend(w16_le(irq))
    out.extend(data)
    out.extend(w16_le(wkc))
    return bytes(out)


def parse_datagrams(pdu: bytes) -> List[Tuple[int, int, int, int, int, int, bytearray, int]]:
    """Parse all datagrams from a PDU. Returns list of (cmd, idx, adp, ado, dlen, irq, data, wkc)."""
    out: List[Tuple[int, int, int, int, int, int, bytearray, int]] = []
    off = 0
    while off + UL_ECAT_DGRAM_HDR_LEN + UL_ECAT_DGRAM_WKC_LEN <= len(pdu):
        cmd = pdu[off]
        idx = pdu[off + 1]
        adp = r16_le(pdu, off + 2)
        ado = r16_le(pdu, off + 4)
        dlen = r16_le(pdu, off + 6) & 0x7FF
        irq = r16_le(pdu, off + 8)
        if off + UL_ECAT_DGRAM_HDR_LEN + dlen + UL_ECAT_DGRAM_WKC_LEN > len(pdu):
            break
        data = bytearray(pdu[off + UL_ECAT_DGRAM_HDR_LEN : off + UL_ECAT_DGRAM_HDR_LEN + dlen])
        wkc = r16_le(pdu, off + UL_ECAT_DGRAM_HDR_LEN + dlen)
        out.append((cmd, idx, adp, ado, dlen, irq, data, wkc))
        off += UL_ECAT_DGRAM_HDR_LEN + dlen + UL_ECAT_DGRAM_WKC_LEN
    return out


def parse_first_datagram(pdu: bytes) -> Tuple[int, int, int, int, int, int, bytes, int]:
    """Convenience: parse only the first datagram, raise on error."""
    dgs = parse_datagrams(pdu)
    if not dgs:
        raise ValueError("no datagrams in PDU")
    cmd, idx, adp, ado, dlen, irq, data, wkc = dgs[0]
    return cmd, idx, adp, ado, dlen, irq, bytes(data), wkc


def build_eth_frame(dst: bytes, src: bytes, ec_payload: bytes) -> bytes:
    ec_len_field = 2 + len(ec_payload)
    frame = bytearray()
    frame.extend(dst)
    frame.extend(src)
    frame.extend(struct.pack("!H", ETH_P_ECAT))
    frame.extend(w16_le(ec_len_field))
    frame.extend(w16_le(0))
    frame.extend(ec_payload)
    return bytes(frame)


def parse_eth_ec_payload(frame: bytes) -> Tuple[bytes, bytes]:
    """Return (datagram_area, src_mac_from_frame)."""
    if len(frame) < 18:
        raise ValueError("frame too short")
    et = (frame[12] << 8) | frame[13]
    if et != ETH_P_ECAT:
        raise ValueError("not EtherCAT ethertype")
    ec_len = r16_le(frame, 14)
    if ec_len < 2:
        raise ValueError("bad ec_len")
    dgram_area_len = ec_len - 2
    if len(frame) < 18 + dgram_area_len:
        raise ValueError("truncated PDU")
    pdu = frame[18 : 18 + dgram_area_len]
    src_mac = frame[6:12]
    return pdu, src_mac
