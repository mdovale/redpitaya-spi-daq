#!/usr/bin/env python3
"""
BSD 3-Clause License

Copyright (c) 2026, Miguel Dovale (University of Arizona)

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

from __future__ import annotations

import argparse
import os
import struct
import sys
from typing import BinaryIO, TextIO


# ads1278_dump record layout:
#   uint64_t seq (LE)
#   uint64_t tstamp_ns (LE)
#   int32_t ch[8] (LE), sign-extended from 24-bit samples
REC = struct.Struct("<QQ8i")
CHANNELS = 8


def _default_out_path(in_path: str, fmt: str) -> str:
    base, _ext = os.path.splitext(in_path)
    return f"{base}.{fmt}"


def _code_to_volts(code: int, vref: float) -> float:
    """
    Convert signed ADC code to volts.

    Assumption: bipolar 24-bit data, sign-extended to int32, with full-scale
    corresponding to ±Vref. This yields:
        volts = code / 2^23 * Vref
    where code is in [-2^23, 2^23-1].
    """
    return (float(code) / float(1 << 23)) * float(vref)


def _write_header(out: TextIO, delim: str, to_volts: bool) -> None:
    cols = ["seq", "tstamp_ns"]
    if to_volts:
        cols += [f"ch{i+1}_V" for i in range(CHANNELS)]
    else:
        cols += [f"ch{i+1}" for i in range(CHANNELS)]
    out.write(delim.join(cols) + "\n")


def _unpack_stream(inp: BinaryIO, out: TextIO, *, fmt: str, to_volts: bool, vref: float) -> int:
    delim = "\t" if fmt == "tsv" else ","
    _write_header(out, delim, to_volts)

    n = 0
    while True:
        chunk = inp.read(REC.size)
        if not chunk:
            break
        if len(chunk) != REC.size:
            raise RuntimeError(f"Truncated record at byte {n * REC.size}: got {len(chunk)} bytes")

        seq, tstamp_ns, *ch = REC.unpack(chunk)

        if to_volts:
            vals = [_code_to_volts(x, vref) for x in ch]
            row = [str(seq), str(tstamp_ns)] + [f"{v:.12g}" for v in vals]
        else:
            row = [str(seq), str(tstamp_ns)] + [str(x) for x in ch]

        out.write(delim.join(row) + "\n")
        n += 1

    return n


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(
        description="Unpack ads1278_dump --out binary records into TSV/CSV text."
    )
    p.add_argument("input", help="Path to binary capture file produced by ads1278_dump --out")
    p.add_argument(
        "-o",
        "--output",
        default=None,
        help="Output text file path (default: <input>.tsv or <input>.csv)",
    )
    p.add_argument(
        "--format",
        choices=("tsv", "csv"),
        default="tsv",
        help="Output delimiter format (default: tsv)",
    )
    p.add_argument(
        "--to-volts",
        action="store_true",
        help="Convert channel samples to volts (see --vref)",
    )
    p.add_argument(
        "--vref",
        type=float,
        default=2.5,
        help="Reference voltage used for --to-volts conversion (default: 2.5)",
    )

    args = p.parse_args(argv)

    out_path = args.output or _default_out_path(args.input, args.format)

    with open(args.input, "rb") as inp, open(out_path, "w", encoding="utf-8", newline="\n") as out:
        n = _unpack_stream(inp, out, fmt=args.format, to_volts=args.to_volts, vref=args.vref)

    print(f"Wrote {n} record(s) to {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

