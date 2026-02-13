<!--
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
-->

# ADS1278 Capture Output Format (M0)

This document defines how `ads1278_dump` interprets SPI bytes and writes records when `--out` is used.

## Raw SPI frame layout

- One conversion frame is read as exactly **24 bytes**.
- Channel order is fixed-position TDM on DOUT1:
  - bytes `0..2` -> CH1
  - bytes `3..5` -> CH2
  - bytes `6..8` -> CH3
  - bytes `9..11` -> CH4
  - bytes `12..14` -> CH5
  - bytes `15..17` -> CH6
  - bytes `18..20` -> CH7
  - bytes `21..23` -> CH8
- Each channel sample is 24-bit two's-complement, MSB-first:
  - `raw24 = (b0 << 16) | (b1 << 8) | b2`
  - if `raw24 & 0x800000`, software sign-extends to 32-bit.

## In-memory frame representation

`ads1278_read_frame()` returns:

- `seq`: 64-bit frame sequence counter (increments by 1 per accepted frame)
- `tstamp_ns`: monotonic timestamp in nanoseconds (`CLOCK_MONOTONIC`) taken at DRDY servicing time
- `ch[8]`: signed 32-bit samples, where each value is sign-extended from ADC 24-bit sample

## Binary file layout (`--out`)

Each output record is fixed-width and little-endian:

- `seq` : `u64` (8 bytes)
- `tstamp_ns` : `u64` (8 bytes)
- `ch[0..7]` : `int32` x 8 (32 bytes)

Total bytes per record: **48 bytes**.

There is no file header in M0; the file is a flat stream of 48-byte records.

## Sanity checks during validation

- Sequence values should be strictly monotonic.
- Raw frame hex (`--hex N`) should show stable byte alignment.
- Known input conditions should produce plausible signed values:
  - near-zero input -> codes near 0
  - positive input -> positive code trend
  - negative input -> negative code trend
