#!/usr/bin/env python3
"""
EtherCAT L2 responder for tests: emulates a single slave with Beckhoff EL7201-like identity.

Reference device (public product data): EL7201 1-channel servo interface (resolver).
Override identity with env: UL_ECAT_SIM_VENDOR_ID, UL_ECAT_SIM_PRODUCT_CODE, etc. (hex).

Requires root or CAP_NET_RAW. Typical: veth pair, bind to one end, run master on peer.

Usage:
  sudo ./ecat_slave_sim.py [-v|--verbose] [iface]

  -v   Log each processed datagram (WKC, ADP, ADO).

Environment:
  UL_ECAT_SLAVE_IFACE - default interface if argv omitted
"""
from __future__ import annotations

import os
import socket
import struct
import sys

ETH_P_ECAT = 0x88A4
UL_ECAT_CMD_FPRD = 0x04
UL_ECAT_CMD_FPWR = 0x05
UL_ECAT_CMD_APWR = 0x02

# Beckhoff EL7201 representative identity (public datasheets / ESI family EL72x1)
DEFAULT_VENDOR = 0x00000002
DEFAULT_PRODUCT = 0x1C213052
DEFAULT_REVISION = 0x00010000
DEFAULT_SERIAL = 0x00000001

ESC_REG_STADR = 0x0010
ESC_REG_ALCTL = 0x0120
ESC_REG_ALSTAT = 0x0130


def _env_u32(name: str, default: int) -> int:
    v = os.environ.get(name)
    return int(v, 0) if v else default


def le16(b: bytes, off: int) -> int:
    return b[off] | (b[off + 1] << 8)


def w16_le(v: int) -> bytes:
    return bytes([v & 0xFF, (v >> 8) & 0xFF])


def u32_le(v: int) -> bytes:
    return struct.pack("<I", v & 0xFFFFFFFF)


def parse_datagrams(pdu: bytes) -> list[tuple[int, int, int, int, int, int, bytes, int]]:
    """Return list of (cmd, idx, adp, ado, dlen, irq, data, wkc_rx)."""
    out = []
    off = 0
    while off + 12 <= len(pdu):
        cmd = pdu[off]
        idx = pdu[off + 1]
        adp = le16(pdu, off + 2)
        ado = le16(pdu, off + 4)
        lword = le16(pdu, off + 6)
        dlen = lword & 0x7FF
        irq = le16(pdu, off + 8)
        hdr = 10
        if off + hdr + dlen + 2 > len(pdu):
            break
        data = bytearray(pdu[off + hdr : off + hdr + dlen])
        wkc_rx = le16(pdu, off + hdr + dlen)
        out.append((cmd, idx, adp, ado, dlen, irq, data, wkc_rx))
        off += hdr + dlen + 2
    return out


class ServoSim:
    def __init__(self, verbose: bool = False) -> None:
        self.verbose = verbose
        self.station_adr: int | None = None
        self.al_state = 1  # INIT
        self.vendor = _env_u32("UL_ECAT_SIM_VENDOR_ID", DEFAULT_VENDOR)
        self.product = _env_u32("UL_ECAT_SIM_PRODUCT_CODE", DEFAULT_PRODUCT)
        self.revision = _env_u32("UL_ECAT_SIM_REVISION", DEFAULT_REVISION)
        self.serial = _env_u32("UL_ECAT_SIM_SERIAL", DEFAULT_SERIAL)

    def handle_one(self, cmd: int, idx: int, adp: int, ado: int, dlen: int, irq: int, data: bytearray, wkc_rx: int) -> bytes:
        wkc_out = wkc_rx + 1

        if cmd == UL_ECAT_CMD_APWR and ado == ESC_REG_STADR and dlen >= 2:
            self.station_adr = le16(bytes(data), 0)
        elif cmd == UL_ECAT_CMD_FPWR and ado == ESC_REG_ALCTL and dlen >= 2:
            val = le16(bytes(data), 0)
            req = val & 0x000F
            ack = val & 0x0010
            if not ack and req in (1, 2, 3, 4, 8):
                self.al_state = req
            # ACK write: no state change in this minimal model
            _ = ack
        elif cmd == UL_ECAT_CMD_FPRD:
            if self.station_adr is not None and adp != self.station_adr:
                wkc_out = 0
            else:
                if ado == ESC_REG_ALSTAT and dlen >= 2:
                    data[0:2] = w16_le(self.al_state & 0x0F)
                elif ado == 0x0012 and dlen >= 4:
                    data[0:4] = u32_le(self.vendor)
                elif ado == 0x0016 and dlen >= 4:
                    data[0:4] = u32_le(self.product)
                elif ado == 0x001A and dlen >= 4:
                    data[0:4] = u32_le(self.revision)
                elif ado == 0x001E and dlen >= 4:
                    data[0:4] = u32_le(self.serial)

        if self.verbose:
            cname = {UL_ECAT_CMD_APWR: "APWR", UL_ECAT_CMD_FPWR: "FPWR", UL_ECAT_CMD_FPRD: "FPRD"}.get(
                cmd, f"CMD{cmd:02X}"
            )
            print(
                f"[ecat_slave_sim] {cname} adp=0x{adp:04X} ado=0x{ado:04X} dlen={dlen} -> WKC={wkc_out} "
                f"(station={self.station_adr})",
                flush=True,
            )

        outdg = bytearray()
        outdg.append(cmd)
        outdg.append(idx & 0xFF)
        outdg.extend(w16_le(adp))
        outdg.extend(w16_le(ado))
        outdg.extend(w16_le(dlen & 0x7FF))
        outdg.extend(w16_le(irq))
        outdg.extend(data)
        outdg.extend(w16_le(wkc_out))
        return bytes(outdg)


def build_reply(rx: bytes, sim: ServoSim) -> bytes | None:
    if len(rx) < 18:
        return None
    et = (rx[12] << 8) | rx[13]
    if et != ETH_P_ECAT:
        return None
    ec_len = le16(rx, 14)
    if ec_len < 2 or len(rx) < 18 + ec_len - 2:
        return None
    pdu = rx[18 : 18 + ec_len - 2]
    dgs = parse_datagrams(pdu)
    if not dgs:
        return None

    out_parts: list[bytes] = []
    for cmd, idx, adp, ado, dlen, irq, data, wkc_rx in dgs:
        data_b = bytearray(data)
        out_parts.append(sim.handle_one(cmd, idx, adp, ado, dlen, irq, data_b, wkc_rx))

    ec_payload = b"".join(out_parts)
    ec_field = 2 + len(ec_payload)
    frame = bytearray()
    frame.extend(rx[6:12])
    frame.extend(rx[0:6])
    frame.extend(struct.pack("!H", ETH_P_ECAT))
    frame.extend(w16_le(ec_field))
    frame.extend(w16_le(0))
    frame.extend(ec_payload)
    return bytes(frame)


def main() -> int:
    args = sys.argv[1:]
    verbose = False
    while args and args[0] in ("-v", "--verbose"):
        verbose = True
        args = args[1:]
    ifname = args[0] if args else os.environ.get("UL_ECAT_SLAVE_IFACE", "eth0")
    sim = ServoSim(verbose=verbose)
    s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(ETH_P_ECAT))
    s.bind((ifname, 0))
    print(
        f"ecat_slave_sim (EL7201-like) on {ifname}: "
        f"vendor=0x{sim.vendor:08X} product=0x{sim.product:08X}",
        flush=True,
    )
    while True:
        rx, _ = s.recvfrom(2048)
        rep = build_reply(rx, sim)
        if rep:
            try:
                s.send(rep)
            except OSError:
                pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
