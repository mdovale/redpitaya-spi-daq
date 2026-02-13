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

# ADS1278EVM Wiring (M0)

This document captures the M0 bring-up wiring between Red Pitaya (STEMlab 125-14 PRO Gen 2) and ADS1278EVM for TDM readout.

## Required signal mapping

| Function | Red Pitaya side | ADS1278EVM side | Notes |
| --- | --- | --- | --- |
| SPI clock | E2 SPI SCK (Pin 5) | SCLK | Required |
| SPI data in | E2 SPI MISO (Pin 4) | DOUT1 | TDM stream CH1..CH8 |
| Ground | RP GND | EVM GND | Common reference required |
| DRDY event | RP GPIO input (line TBD) | /DRDY_FSYNC | Falling-edge trigger |
| SYNC control | RP GPIO output (line TBD) | /SYNC | Active-low pulse at startup |

## DIN and CS handling

- **DIN**: preferred setup is DIN strapped to GND on EVM (if not daisy-chaining).
- **Alternative**: wire RP MOSI to DIN and transmit zeros.
- **CS**: ADS1278 TDM readout does not require chip select. RP CS can be left unconnected.

## ADS1278EVM strap/jumper intent

For M0 the board should be configured for:
- SPI output
- TDM on DOUT1
- Fixed-position channel ordering (CH1..CH8 each frame)

Record exact switch/jumper positions used during bring-up in the table below.

## RP GPIO assignment record (fill on target system)

Fill these with the exact GPIO mapping from your RP OS image.

| Role | sysfs GPIO number | Connection |
| --- | --- | --- |
| DRDY | `TBD` | EVM /DRDY_FSYNC -> RP input |
| SYNC | `TBD` | RP output -> EVM /SYNC |

## Software examples

sysfs backend:

```bash
./ads1278_dump \
  --spidev /dev/spidev2.0 \
  --drdy 968 \
  --sync 969 \
  --frames 10000 \
  --print
```

## Validation notes (to fill during lab bring-up)

- RP board and OS image:
- ADS1278EVM power/setup:
- Input condition for plausibility test (grounded/known source):
- Observed DRDY behavior:
- Any wiring photos or links:
