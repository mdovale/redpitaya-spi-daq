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

This folder currently provides M0 bring-up support for ADS1278 on Red Pitaya ARM userspace:

- HAL: `include/ads1278.h`, `src/spi/ads1278/ads1278.c`
- capture tool: `ads1278_dump`

## Build

libgpiod backend enabled (preferred):

```bash
make
```

sysfs fallback only:

```bash
make USE_LIBGPIOD=0
```

Cross-compile example:

```bash
make CC=arm-linux-gnueabihf-gcc
```

## Quick run examples

libgpiod:

```bash
./ads1278_dump --spidev /dev/spidev2.0 --drdy gpiochip0:12 --sync gpiochip0:13 --frames 10000 --print
```

sysfs fallback:

```bash
./ads1278_dump --spidev /dev/spidev2.0 --drdy 904 --sync 905 --frames 10000 --print
```
