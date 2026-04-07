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
from typing import Optional

ETH_P_ECAT = 0x88A4

UL_ECAT_CMD_APWR = 0x02
UL_ECAT_CMD_FPRD = 0x04

UL_ECAT_PDU_HDR_LEN = 4
UL_ECAT_DGRAM_HDR_LEN = 10
UL_ECAT_DGRAM_WKC_LEN = 2

ESC_REG_STADR = 0x0010
ESC_REG_VENDOR = 0x0012
ESC_REG_PRODUCT = 0x0016
ESC_REG_REV = 0x001A
ESC_REG_SERIAL = 0x001E


def _w16_le(v: int) -> bytes:
    return bytes([v & 0xFF, (v >> 8) & 0xFF])


def _pack_len_irq(data_len: int, irq: int = 0) -> int:
    return data_len & 0x7FF


def dgram_encode(
    cmd: int,
    index: int,
    adp: int,
    ado: int,
    data_len: int,
    irq: int,
    wkc_out: int,
    data: Optional[bytes],
) -> bytes:
    out = bytearray()
    out.append(cmd & 0xFF)
    out.append(index & 0xFF)
    out.extend(_w16_le(adp))
    out.extend(_w16_le(ado))
    out.extend(_w16_le(_pack_len_irq(data_len, irq)))
    out.extend(_w16_le(irq))
    if data_len > 0:
        if data is not None:
            out.extend(data[:data_len].ljust(data_len, b"\0"))
        else:
            out.extend(b"\0" * data_len)
    out.extend(_w16_le(wkc_out))
    return bytes(out)


def build_eth_frame(dst: bytes, src: bytes, ec_payload: bytes) -> bytes:
    ec_len_field = 2 + len(ec_payload)
    frame = bytearray()
    frame.extend(dst)
    frame.extend(src)
    frame.extend(struct.pack("!H", ETH_P_ECAT))
    frame.extend(_w16_le(ec_len_field))
    frame.extend(_w16_le(0))
    frame.extend(ec_payload)
    return bytes(frame)


def parse_eth_ec_payload(frame: bytes) -> tuple[bytes, bytes]:
    """Return (ec_datagram_area, master_src_mac_from_rx_frame)."""
    if len(frame) < 18:
        raise ValueError("frame too short")
    et = (frame[12] << 8) | frame[13]
    if et != ETH_P_ECAT:
        raise ValueError("not EtherCAT ethertype")
    ec_len = struct.unpack_from("<H", frame, 14)[0]
    if ec_len < 2:
        raise ValueError("bad ec_len")
    dgram_area_len = ec_len - 2
    if len(frame) < 18 + dgram_area_len:
        raise ValueError("truncated PDU")
    pdu = frame[18 : 18 + dgram_area_len]
    master_sa = frame[6:12]
    return pdu, master_sa


def parse_first_datagram(pdu: bytes) -> tuple[int, int, int, int, int, bytes, int]:
    if len(pdu) < UL_ECAT_DGRAM_HDR_LEN + UL_ECAT_DGRAM_WKC_LEN:
        raise ValueError("pdu too short")
    cmd = pdu[0]
    idx = pdu[1]
    adp = struct.unpack_from("<H", pdu, 2)[0]
    ado = struct.unpack_from("<H", pdu, 4)[0]
    dlen = struct.unpack_from("<H", pdu, 6)[0] & 0x7FF
    irq = struct.unpack_from("<H", pdu, 8)[0]
    if len(pdu) < UL_ECAT_DGRAM_HDR_LEN + dlen + UL_ECAT_DGRAM_WKC_LEN:
        raise ValueError("bad dgram")
    data = pdu[UL_ECAT_DGRAM_HDR_LEN : UL_ECAT_DGRAM_HDR_LEN + dlen]
    wkc = struct.unpack_from("<H", pdu, UL_ECAT_DGRAM_HDR_LEN + dlen)[0]
    return cmd, idx, adp, ado, dlen, irq, data, wkc


def u32_le(b: bytes) -> int:
    return struct.unpack("<I", b[:4])[0]


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
            val = u32_le(data)
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
