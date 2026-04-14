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

from ecat_wire import (
    ETH_P_ECAT,
    UL_ECAT_CMD_FPRD,
    UL_ECAT_CMD_FPWR,
    UL_ECAT_CMD_APWR,
    DEFAULT_VENDOR,
    DEFAULT_PRODUCT,
    DEFAULT_REVISION,
    DEFAULT_SERIAL,
    ESC_REG_STADR,
    ESC_REG_ALCTL,
    ESC_REG_ALSTAT,
    ESC_REG_ALSTACODE,
    ESC_REG_VENDOR,
    ESC_REG_PRODUCT,
    ESC_REG_REV,
    ESC_REG_SERIAL,
    AL_VALID_TRANSITIONS,
    AL_ERR_INVALID_STATE_CHANGE,
    AL_ERR_BOOTSTRAP_NOT_SUPPORTED,
    AL_STATE_BOOT,
)
from ecat_frame import (
    r16_le as le16,
    w16_le,
    w32_le as u32_le,
    parse_datagrams,
    dgram_repack,
    parse_eth_ec_payload,
    build_eth_frame,
)


def _env_u32(name: str, default: int) -> int:
    v = os.environ.get(name)
    return int(v, 0) if v else default


class ServoSim:
    def __init__(self, verbose: bool = False) -> None:
        self.verbose = verbose
        self.station_adr: int | None = None
        self.al_state = 1  # INIT
        self.al_error = False
        self.al_status_code = 0
        self.vendor = _env_u32("UL_ECAT_SIM_VENDOR_ID", DEFAULT_VENDOR)
        self.product = _env_u32("UL_ECAT_SIM_PRODUCT_CODE", DEFAULT_PRODUCT)
        self.revision = _env_u32("UL_ECAT_SIM_REVISION", DEFAULT_REVISION)
        self.serial = _env_u32("UL_ECAT_SIM_SERIAL", DEFAULT_SERIAL)

    def _process_al_control(self, val: int) -> None:
        req = val & 0x000F
        ack = val & 0x0010
        if ack:
            if req == self.al_state:
                self.al_error = False
                self.al_status_code = 0
            return
        if req == self.al_state:
            return
        if req == AL_STATE_BOOT:
            self.al_error = True
            self.al_status_code = AL_ERR_BOOTSTRAP_NOT_SUPPORTED
            return
        if req == 1 or (self.al_state, req) in AL_VALID_TRANSITIONS:
            self.al_state = req
            self.al_error = False
            self.al_status_code = 0
        else:
            self.al_error = True
            self.al_status_code = AL_ERR_INVALID_STATE_CHANGE

    def handle_one(self, cmd: int, idx: int, adp: int, ado: int, dlen: int, irq: int, data: bytearray, wkc_rx: int) -> bytes:
        wkc_out = wkc_rx + 1

        if cmd == UL_ECAT_CMD_APWR and ado == ESC_REG_STADR and dlen >= 2:
            self.station_adr = le16(bytes(data), 0)
        elif cmd == UL_ECAT_CMD_FPWR and ado == ESC_REG_ALCTL and dlen >= 2:
            self._process_al_control(le16(bytes(data), 0))
        elif cmd == UL_ECAT_CMD_FPRD:
            if self.station_adr is not None and adp != self.station_adr:
                wkc_out = 0
            else:
                if ado == ESC_REG_ALSTAT and dlen >= 2:
                    st = self.al_state & 0x0F
                    if self.al_error:
                        st |= 0x10
                    data[0:2] = w16_le(st)
                elif ado == ESC_REG_ALSTACODE and dlen >= 2:
                    data[0:2] = w16_le(self.al_status_code)
                elif ado == ESC_REG_VENDOR and dlen >= 4:
                    data[0:4] = u32_le(self.vendor)
                elif ado == ESC_REG_PRODUCT and dlen >= 4:
                    data[0:4] = u32_le(self.product)
                elif ado == ESC_REG_REV and dlen >= 4:
                    data[0:4] = u32_le(self.revision)
                elif ado == ESC_REG_SERIAL and dlen >= 4:
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

        return dgram_repack(cmd, idx, adp, ado, dlen, irq, data, wkc_out)


def build_reply(rx: bytes, sim: ServoSim) -> bytes | None:
    try:
        pdu, _ = parse_eth_ec_payload(rx)
    except ValueError:
        return None
    dgs = parse_datagrams(pdu)
    if not dgs:
        return None

    out_parts: list[bytes] = []
    for cmd, idx, adp, ado, dlen, irq, data, wkc_rx in dgs:
        out_parts.append(sim.handle_one(cmd, idx, adp, ado, dlen, irq, bytearray(data), wkc_rx))

    dst = rx[6:12]
    src = rx[0:6]
    return build_eth_frame(dst, src, b"".join(out_parts))


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
