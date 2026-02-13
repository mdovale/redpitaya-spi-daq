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

# ADS1278 M0 Validation Runbook

Use this runbook on the Red Pitaya target to complete the M0 acceptance criteria.

## 1) Build on target

```bash
cd server
make
```

## 2) Capture 10,000 frames (sysfs backend)

```bash
./ads1278_dump \
  --spidev /dev/spidev2.0 \
  --drdy 968 \
  --sync 969 \
  --settle-frames 16 \
  --frames 10000 \
  --out /tmp/ads1278_10k.bin \
  --hex 4
```

## 3) Equivalent explicit sysfs form

```bash
./ads1278_dump \
  --spidev /dev/spidev2.0 \
  --drdy sysfs:968 \
  --sync sysfs:969 \
  --settle-frames 16 \
  --frames 10000 \
  --out /tmp/ads1278_10k.bin \
  --hex 4
```

## 4) Acceptance checklist

- [ ] No `ads1278_read_frame` timeout during 10k capture.
- [ ] Sequence count reaches 10,000 records.
- [ ] First hex dumps look frame-aligned across channels.
- [ ] Sample values are plausible for known input condition.
- [ ] SYNC pulse path works and post-SYNC settle discard is active.

## 5) Record results

- RP image / kernel:
- ADC input condition:
- DRDY and SYNC lines used:
- Capture command used:
- Outcome:
