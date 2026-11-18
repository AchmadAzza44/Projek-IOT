#!/usr/bin/env python3
"""
Minggu 4 — Edge Gateway Serial Ingestion
Membaca baris JSON dari:
  1. File log Wokwi yang disimpan pengguna (--file wokwi_log.txt)
  2. Port serial fisik ESP32 (--port COM3 / /dev/ttyUSB0)
Lalu membandingkan ukuran payload JSON vs CBOR vs Protobuf secara real-time.
"""

import argparse
import json
import sys
from pathlib import Path

try:
    import cbor2
except ImportError:
    sys.exit("Install dulu: pip install cbor2 pyserial")

from gateway_benchmark import encode_json, encode_cbor, encode_protobuf


def iter_json_lines_file(path: Path):
    with path.open(encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if line.startswith("{") and line.endswith("}"):
                try:
                    yield json.loads(line)
                except json.JSONDecodeError:
                    continue


def iter_json_lines_serial(port: str, baud: int):
    import serial
    ser = serial.Serial(port, baud, timeout=5)
    print(f"[Serial] Terbuka di {port} @ {baud}")
    try:
        while True:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if line.startswith("{") and line.endswith("}"):
                try:
                    yield json.loads(line)
                except json.JSONDecodeError:
                    continue
    finally:
        ser.close()


def report(d: dict):
    # Pastikan field wajib ada (sesuai skema ESP32 Minggu 3)
    for field in ("suhu", "tekanan", "altitude", "seq", "device_id"):
        d.setdefault(field, 0 if field != "device_id" else "esp32-01")

    sizes = {
        "JSON":     len(encode_json(d)),
        "CBOR":     len(encode_cbor(d)),
        "Protobuf": len(encode_protobuf(d)),
    }
    print(
        f"[RX] boot={d.get('boot','?')} seq={d['seq']:<4} "
        f"T={d['suhu']:>5.2f}C  P={d['tekanan']:>7.2f}hPa  "
        f"Alt={d['altitude']:>5.1f}m  |  "
        f"JSON={sizes['JSON']}B  CBOR={sizes['CBOR']}B  "
        f"Protobuf={sizes['Protobuf']}B"
    )


def main():
    ap = argparse.ArgumentParser()
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--file", help="Path log Wokwi")
    src.add_argument("--port", help="Serial port (COM3 / /dev/ttyUSB0)")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    gen = (iter_json_lines_file(Path(args.file))
           if args.file else
           iter_json_lines_serial(args.port, args.baud))

    print("[Gateway] Menunggu telemetri JSON...\n")
    n = 0
    try:
        for payload in gen:
            report(payload)
            n += 1
    except KeyboardInterrupt:
        pass
    print(f"\n[Gateway] {n} pesan diterima.")


if __name__ == "__main__":
    main()