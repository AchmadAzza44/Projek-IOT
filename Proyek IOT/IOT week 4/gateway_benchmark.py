#!/usr/bin/env python3
"""
Minggu 4 — Benchmark Ukuran Payload: JSON vs CBOR vs Protobuf v3
Membuktikan klaim "Protobuf hemat ~68%" dengan data telemetri realistis.

Mode:
  python gateway_benchmark.py                     # synthetic (default)
  python gateway_benchmark.py --n 500             # 500 sampel
"""

import argparse
import csv
import json
import random
import struct
import time
from datetime import datetime

try:
    import cbor2
except ImportError:
    raise SystemExit("Install dulu: pip install cbor2")

# ============================================================
# ENCODER PROTOBUF MANUAL (tanpa protoc)
# Skema Telemetry:
#   float  suhu     = 1;
#   float  tekanan  = 2;
#   float  altitude = 3;
#   int32  seq      = 4;
#   string device_id= 5;
# ============================================================
def encode_protobuf(d: dict) -> bytes:
    buf = bytearray()
    # Field 1 (float, wire-type 5)
    buf += bytes([(1 << 3) | 5]); buf += struct.pack("<f", float(d["suhu"]))
    # Field 2 (float, wire-type 5)
    buf += bytes([(2 << 3) | 5]); buf += struct.pack("<f", float(d["tekanan"]))
    # Field 3 (float, wire-type 5)
    buf += bytes([(3 << 3) | 5]); buf += struct.pack("<f", float(d["altitude"]))
    # Field 4 (int32, wire-type 0 → varint)
    buf += bytes([(4 << 3) | 0])
    seq = int(d["seq"])
    while seq > 0x7F:
        buf += bytes([(seq & 0x7F) | 0x80])
        seq >>= 7
    buf += bytes([seq])
    # Field 5 (string, wire-type 2)
    buf += bytes([(5 << 3) | 2])
    did = str(d["device_id"]).encode("utf-8")
    buf += bytes([len(did)]); buf += did
    return bytes(buf)


def encode_json(d: dict) -> bytes:
    return json.dumps(d, separators=(",", ":")).encode("utf-8")


def encode_cbor(d: dict) -> bytes:
    return cbor2.dumps(d)


# ============================================================
# Generator data telemetri realistis (rentang khas UNIB)
# ============================================================
def sample_telemetry(seq: int) -> dict:
    return {
        "device_id": "esp32-01",
        "seq":       seq,
        "suhu":      round(random.uniform(26.0, 33.0), 2),
        "tekanan":   round(random.uniform(1008.0, 1014.0), 2),
        "altitude":  round(random.uniform(10.0, 60.0), 1),
    }


# ============================================================
# Benchmark utama
# ============================================================
def run_benchmark(n: int = 200, csv_path: str = "benchmark_results.csv"):
    rows = []
    total = {"JSON": 0, "CBOR": 0, "Protobuf": 0}

    print(f"[Benchmark] Menguji {n} sampel telemetri...\n")
    t0 = time.perf_counter()
    for i in range(1, n + 1):
        d = sample_telemetry(i)
        sizes = {
            "JSON":     len(encode_json(d)),
            "CBOR":     len(encode_cbor(d)),
            "Protobuf": len(encode_protobuf(d)),
        }
        for k in total:
            total[k] += sizes[k]
        rows.append({**d, **{f"{k.lower()}_bytes": v for k, v in sizes.items()}})

    elapsed = (time.perf_counter() - t0) * 1000

    # Rata-rata
    avg = {k: total[k] / n for k in total}

    # Laporan
    print("=" * 60)
    print(f"{'Format':<10}{'Rata-rata (Byte)':<20}{'vs JSON':<15}")
    print("-" * 60)
    for fmt, val in avg.items():
        ratio = (val / avg["JSON"]) * 100
        saving = 100 - ratio
        print(f"{fmt:<10}{val:<20.1f}{saving:+5.1f}% lebih kecil")
    print("-" * 60)
    print(f"Total waktu encode {n} sampel : {elapsed:.2f} ms")
    print("=" * 60)

    # Simpan CSV
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=rows[0].keys())
        writer.writeheader(); writer.writerows(rows)
    print(f"\n[OK] Detail {n} sampel tersimpan di: {csv_path}")

    # Verifikasi Protobuf dengan blackbox (opsional, tanpa protoc)
    print("\n[Verifikasi Protobuf] Header field yang dihasilkan:")
    sample = encode_protobuf(sample_telemetry(1))
    print(f"  {sample.hex()[:80]}...")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=200, help="Jumlah sampel")
    ap.add_argument("--csv", default="benchmark_results.csv")
    args = ap.parse_args()
    run_benchmark(args.n, args.csv)