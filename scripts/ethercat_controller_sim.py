#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Stateful EtherCAT controller simulator for local harness tests (not real-time).

Builds the same scan sequence as the C master (APWR station address, FPRD identity
registers) using the ETG wire layout matching src/common/ul_ecat_frame.c.
"""
from __future__ import annotations

import argparse
import socket
import struct
import sys
from dataclasses import dataclass

from ecat_wire import (
    UL_ECAT_CMD_APWR,
    UL_ECAT_CMD_FPRD,
    ESC_REG_STADR,
    ESC_REG_VENDOR,
    ESC_REG_PRODUCT,
    ESC_REG_REV,
    ESC_REG_SERIAL,
)
from ecat_frame import (
    w16_le as _w16_le,
    r32_le as _r32_le,
    dgram_encode,
    build_eth_frame,
    parse_eth_ec_payload,
    parse_first_datagram,
)


@dataclass
class ScanResult:
    vendor_id: int
    product_code: int
    revision: int
    serial: int


def _send_frame(sock: socket.socket, frame: bytes) -> None:
    hdr = struct.pack("!I", len(frame))
    sock.sendall(hdr)
    sock.sendall(frame)


def _recv_frame(sock: socket.socket) -> bytes:
    buf = _recv_exact(sock, 4)
    n = struct.unpack("!I", buf)[0]
    if n == 0 or n > 4096:
        raise ConnectionError(f"bad frame length {n}")
    return _recv_exact(sock, n)


def _recv_exact(sock: socket.socket, n: int) -> bytes:
    parts: list[bytes] = []
    got = 0
    while got < n:
        chunk = sock.recv(n - got)
        if not chunk:
            raise ConnectionError("EOF")
        parts.append(chunk)
        got += len(chunk)
    return b"".join(parts)


def run_identity_scan(
    host: str,
    port: int,
    station: int = 0x1000,
    master_mac: bytes = b"\x02\x00\x00\x00\x00\x02",
    slave_mac: bytes = b"\x02\x00\x00\x00\x00\x01",
) -> ScanResult:
    """
    Connect to harness, perform APWR station + four FPRD identity reads (same logical steps as master scan).
    """
    frames: list[bytes] = [
        build_eth_frame(
            slave_mac,
            master_mac,
            dgram_encode(UL_ECAT_CMD_APWR, 0, 0, ESC_REG_STADR, 2, 0, 0, _w16_le(station)),
        )
    ]
    for ado in (ESC_REG_VENDOR, ESC_REG_PRODUCT, ESC_REG_REV, ESC_REG_SERIAL):
        frames.append(
            build_eth_frame(
                slave_mac,
                master_mac,
                dgram_encode(UL_ECAT_CMD_FPRD, 0, station, ado, 4, 0, 0, None),
            )
        )

    vendor = product = rev = serial = 0
    with socket.create_connection((host, port), timeout=5.0) as sock:
        for i, fr in enumerate(frames):
            _send_frame(sock, fr)
            rx = _recv_frame(sock)
            pdu, _ = parse_eth_ec_payload(rx)
            _, _, _, _, _, _, data, wkc = parse_first_datagram(pdu)
            if wkc < 1:
                step = "APWR" if i == 0 else f"FPRD step {i}"
                raise RuntimeError(f"WKC {wkc} on {step}")
            if i == 0:
                continue
            val = _r32_le(data)
            if i == 1:
                vendor = val
            elif i == 2:
                product = val
            elif i == 3:
                rev = val
            else:
                serial = val

    return ScanResult(vendor_id=vendor, product_code=product, revision=rev, serial=serial)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("-p", "--port", type=int, default=9234)
    args = p.parse_args()
    try:
        r = run_identity_scan(args.host, args.port)
    except OSError as e:
        print(f"Connection failed: {e}", file=sys.stderr)
        return 1
    except (RuntimeError, ValueError, ConnectionError) as e:
        print(f"Scan failed: {e}", file=sys.stderr)
        return 1
    print(
        f"vendor=0x{r.vendor_id:08X} product=0x{r.product_code:08X} "
        f"rev=0x{r.revision:08X} serial=0x{r.serial:08X}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
