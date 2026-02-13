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

# Server (M0)

This folder contains the M0 ADS1278 bring-up implementation for Red Pitaya ARM userspace.
At this stage, the goal is to validate reliable DRDY-aligned SPI frame capture before adding
network streaming.

## What is implemented

- HAL API: `include/ads1278.h`
- HAL implementation: `src/spi/ads1278/ads1278.c`
- capture utility: `tools/ads1278_dump.c`
- local build: `Makefile`

M0 captures one ADS1278 TDM frame per DRDY event:

- waits for `/DRDY` falling edge
- performs one 24-byte SPI transfer
- parses 8 channels of signed 24-bit samples into `int32_t`
- records sequence counter and monotonic timestamp

Related docs:

- `docs/ads1278_wiring.md`
- `docs/ads1278_output.md`
- `docs/ads1278_validation.md`

## Source layout

```text
server/
  include/ads1278.h
  src/spi/ads1278/ads1278.c
  tools/ads1278_dump.c
  Makefile
```

## Build

Build (sysfs GPIO backend):

```bash
make
```

Cross-compile example:

```bash
make CC=arm-linux-gnueabihf-gcc
```

Notes:

- This codebase now uses sysfs GPIO only.
- `--drdy` and `--sync` take global GPIO numbers.

## Build for Red Pitaya (Docker)

From the **repository root**, build an ARM ELF binary without a local arm toolchain:

```bash
./server-build-docker.sh
```

Output: `build-docker/server`. Options: `--rebuild` (rebuild image), `--shell` (shell in builder), `--help`.

## Deploy over network

From the **repository root**, copy the built server to a Red Pitaya (or set `REDPITAYA_IP` / `RP_IP`):

```bash
./server-deploy.sh --ip <IP-or-hostname>
```

Binary is installed to `/usr/local/bin/server` by default; use `--target-dir` and `--user` as needed.

## ads1278_dump CLI

Minimum invocation requires DRDY endpoint:

```bash
./ads1278_dump --drdy <gpio_number>
```

Common options:

- `--spidev` (default `/dev/spidev2.0`)
- `--sclk-hz` (default `1000000`)
- `--spi-mode` (default `0`)
- `--drdy` DRDY endpoint (required)
- `--sync` SYNC endpoint (required unless `--no-sync`)
- `--no-sync` disable startup sync pulse
- `--settle-frames` discard N frames after SYNC pulse
- `--drdy-timeout-ms` DRDY wait timeout (default `2000`)
- `--frames` number of frames to capture (default `1000`)
- `--out` write binary records (`seq`, `tstamp_ns`, `ch[8]`)
- `--print` pretty-print each frame
- `--hex` print raw hex for first N SPI frames

Run `./ads1278_dump --help` for full usage.

## DRDY and SYNC behavior (as implemented)

This section describes the behavior implemented in `src/spi/ads1278/ads1278.c`.

### DRDY (`--drdy`)

`--drdy` is mandatory and defines where the HAL waits for conversion-ready edges.

Accepted formats:

- `N` (example: `968`) -> sysfs global GPIO number
- `sysfs:N` (example: `sysfs:968`) -> equivalent explicit form

How it works:

- DRDY line is configured as input with **falling-edge** events.
- `ads1278_read_frame()` blocks on DRDY event wait.
- Timeout is controlled by `--drdy-timeout-ms` (`drdy_timeout_ms` in HAL config).
- On timeout, call fails with `ETIMEDOUT`.
- After an edge, HAL performs one 24-byte SPI transfer and parses CH1..CH8.

Backend details:

- sysfs path configures `edge=falling` and uses `poll(POLLPRI|POLLERR)` on `value`.

### SYNC (`--sync`, `--no-sync`, `--settle-frames`)

SYNC is used to align startup capture and is active-low.

Behavior:

- If SYNC is enabled (default), `--sync` is required.
- In `ads1278_start()`, HAL drives SYNC low, waits 10 us, then drives high.
- After the pulse, HAL discards `settle_frames` full conversions.
- If SYNC is disabled (`--no-sync`), no pulse/discard step is performed.

Why use settle frames:

- After SYNC, the digital filter pipeline may need a short settling period.
- `--settle-frames` lets you ignore these initial frames.

Recommended practice:

- Keep SYNC enabled when wired.
- Start with `--settle-frames 16` and adjust from empirical behavior/datasheet guidance.

### Timing/overrun warning

HAL reports a warning if SPI transfer time from DRDY exceeds an internal threshold
(currently 5000 us), signaling potential overrun risk.

## Quick run examples

```bash
./ads1278_dump \
  --spidev /dev/spidev2.0 \
  --drdy 968 \
  --sync 969 \
  --settle-frames 16 \
  --frames 10000 \
  --hex 4 \
  --print
```

Alternate explicit sysfs form:

```bash
./ads1278_dump \
  --spidev /dev/spidev2.0 \
  --drdy sysfs:968 \
  --sync sysfs:969 \
  --settle-frames 16 \
  --frames 10000 \
  --print
```

Disable SYNC if not wired:

```bash
./ads1278_dump \
  --spidev /dev/spidev2.0 \
  --drdy 968 \
  --no-sync \
  --frames 10000 \
  --print
```

## Troubleshooting checklist

- `--drdy is required` -> add `--drdy`.
- `--sync is required unless --no-sync is used` -> provide `--sync` or add `--no-sync`.
- timeout waiting for DRDY -> verify ADC clocking, wiring, and chosen GPIO line.
- permission errors on GPIO/spidev -> run with appropriate privileges on target image.
- slow-transfer warnings -> reduce output data rate (clock/config) or investigate CPU load.

## Next step after M0

Once this capture path is stable, the same HAL can be consumed by acquisition buffering
and TCP streaming layers in later milestones (M1+).
