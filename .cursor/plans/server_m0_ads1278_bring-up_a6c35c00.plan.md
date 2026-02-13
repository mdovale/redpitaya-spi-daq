---
name: Server M0 ADS1278 bring-up
overview: "Implement Milestone 0 on the server: ADS1278 HAL (SPI + GPIO, DRDY-driven TDM read), the ads1278_dump capture tool, wiring and output-format documentation, and a build system so the tool runs on Red Pitaya and can capture 10k+ frames reliably."
todos:
  - id: hal-api-impl
    content: "Implement ADS1278 HAL: ads1278.h API and ads1278.c (SPI 24-byte read, libgpiod DRDY/SYNC, parse 8×int32, open/start/read_frame/stop/close)."
    status: completed
  - id: ads1278-dump-tool
    content: "Implement ads1278_dump: CLI (--spidev, --drdy, --sync, --frames, --out, --print), loop, binary output, timeout/overrun handling."
    status: completed
  - id: server-makefile
    content: "Add server Makefile: build HAL and ads1278_dump, link libgpiod; document RP and cross-compile."
    status: completed
  - id: docs-wiring-output
    content: Add docs/ads1278_wiring.md (pin table, GPIO lines) and docs/ads1278_output.md (channel order, sign extension, binary format).
    status: completed
  - id: validate-10k-frames
    content: "Validate on RP: capture ≥10k frames, stable plausible codes, SYNC+settle working; document any sysfs fallback if no libgpiod."
    status: completed
isProject: false
---

# Server development plan: M0 ADS1278 bring-up

This plan covers **server-only** work for **Milestone 0** as defined in [.cursor/plans/M0.md](.cursor/plans/M0.md) and [BLUEPRINT.md](BLUEPRINT.md) (lines 214–362). M1 (TCP + protocol + dummy stream) and the client remain out of scope here.

## Target outcome

- **HAL**: Turn “GPIO /DRDY edges + SPI read” into validated 8-channel `ads1278_frame_t` (seq, timestamp_ns, ch[8] int32).
- **Tool**: `ads1278_dump` that captures N frames to stdout/file and optionally pretty-prints.
- **Docs**: Wiring (RP ↔ ADS1278EVM) and output format (channel order, sign extension, timestamps).
- **Build**: Makefile (or CMake) to build on Red Pitaya or cross-compile; RP Gen 2 default `/dev/spidev2.0`.

## Repository layout (server, M0)

```text
server/
  include/
    ads1278.h          # Public HAL API
  src/
    spi/ads1278/
      ads1278.c        # HAL implementation (SPI + GPIO, parse)
    util/
      log.c, log.h     # Optional; stderr logging for dump tool
  tools/
    ads1278_dump.c     # Standalone capture tool (CLI, loop, --out/--print)
  tests/               # (Optional for M0: host-side parse test only)
  Makefile
docs/
  ads1278_wiring.md    # Pin table RP ↔ EVM, GPIO line numbers
  ads1278_output.md    # Channel order, 24→32 sign extension, timestamp meaning
```

Leave [server/main.c](server/main.c) as-is (scaffold) for now; M1 will replace it with the daemon entrypoint.

## 1. HAL API and implementation

**Header** [server/include/ads1278.h](server/include/ads1278.h) — expose the BLUEPRINT API:

- `ads1278_cfg_t`: spidev_path, sclk_hz, spi_mode, spi_no_cs; drdy_gpiochip + drdy_line; sync_gpiochip, sync_line, use_sync, settle_frames.
- `ads1278_frame_t`: seq (u64), tstamp_ns (u64, CLOCK_MONOTONIC), ch[8] (int32, sign-extended from 24-bit).
- `ads1278_open(cfg)`, `ads1278_start()`, `ads1278_read_frame(out)` (block until DRDY then SPI read), `ads1278_stop()`, `ads1278_close()`.

**Implementation** [server/src/spi/ads1278/ads1278.c](server/src/spi/ads1278/ads1278.c):

- **SPI**: open `spidev`, set mode (default 0), max_speed_hz, bits_per_word=8. Use `SPI_NO_CS` if available (BLUEPRINT: ADS1278 readout does not require CS). Single transfer: 24 bytes RX (TX buffer 0x00 or don’t care); use `struct spi_ioc_transfer` + `SPI_IOC_MESSAGE(1)` as in [_rp_api_reference/spi_example.c](_rp_api_reference/spi_example.c).
- **GPIO**: Prefer **libgpiod** (BLUEPRINT). /DRDY: open chip + line, request falling-edge events, use `gpiod_line_event_wait()` (or poll fd) in `ads1278_read_frame()`. /SYNC: request as output; in `ads1278_start()` optionally pulse low then high; after pulse, discard `settle_frames` reads (call `ads1278_read_frame` and ignore).
- **Parse**: 24 bytes → 8 × int32. Per channel (MSB-first): `raw24 = (b0<<16)|(b1<<8)|b2`; if `raw24 & 0x800000` then sign-extend to 32-bit (e.g. `raw24 | 0xFF000000` or cast via signed 24-bit type). Channel order CH1…CH8 = first three bytes to ch[0], next three to ch[1], etc.
- **Timestamp**: take CLOCK_MONOTONIC (or MONOTONIC_RAW) once after DRDY wait, before or after SPI transfer; store in `tstamp_ns`. Increment `seq` per frame internally.
- **Robustness**: timeout if /DRDY never fires (e.g. configurable timeout in cfg or return error after N seconds). Optionally detect overrun (e.g. time between DRDY and end of transfer too large); report via return code or log.

**Dependencies**: Linux kernel spidev, libgpiod (recommended). If RP OS 2.07 lacks libgpiod, document sysfs GPIO fallback and implement a minimal path (e.g. poll on value file or edge fd) so M0 can still be validated.

## 2. Capture tool: ads1278_dump

**Source** [server/tools/ads1278_dump.c](server/tools/ads1278_dump.c):

- **CLI** (getopt or manual parse):  
`--spidev` (default `/dev/spidev2.0`), `--sclk-hz`, `--spi-mode` (default 0), `--drdy <gpiochip:line>`, `--sync <gpiochip:line>` (optional), `--no-sync` to disable, `--frames N` (required or default e.g. 1000), `--out <path>` (optional binary capture), `--print` (pretty-print each frame), `--settle-frames N` (default per datasheet or 0).
- **Flow**: Parse args → fill `ads1278_cfg_t` → `ads1278_open` → `ads1278_start` (SYNC pulse + settle discard if use_sync) → loop `frames` times: `ads1278_read_frame(&frame)`; if `--out` write binary (e.g. frame struct or raw ch[8] int32 per frame); if `--print` print seq, tstamp_ns, ch[0]…ch[7]. Then `ads1278_stop`; `ads1278_close`.
- **Integrity**: On timeout from `ads1278_read_frame`, abort and report. Optional: measure time between DRDY and end of transfer; if above threshold, log overrun warning. Optional: `--hex N` dump raw 24 bytes for first N frames for debugging.
- **Binary output format**: Document in `docs/ads1278_output.md`. Simple option: each frame = 8 × int32 (little-endian), no header; or frame = seq (u64) + tstamp_ns (u64) + 8×int32. Choose one and document.

## 3. Documentation

- **[docs/ads1278_wiring.md](docs/ads1278_wiring.md)**: Pin-by-pin table (RP E2 and GPIO ↔ EVM J6). Include: E2 SCK → SCLK, E2 MISO ← DOUT1, GND, /DRDY_FSYNC → RP GPIO input (document exact gpiochip and line for your RP build), /SYNC ← RP GPIO output. Note DIN tied low or MOSI→DIN, CS unused. Add short notes or photos if helpful.
- **[docs/ads1278_output.md](docs/ads1278_output.md)**: Channel order (CH1…CH8 = ch[0]…ch[7]), 24-bit two’s complement sign-extended to 32-bit, timestamp meaning (CLOCK_MONOTONIC ns), and binary file format used by `--out`.

## 4. Build system

- **Makefile** in [server/](server/):  
  - Build `libads1278.a` or object files from `src/spi/ads1278/ads1278.c` (and util if added).  
  - Build `ads1278_dump` from `tools/ads1278_dump.c` linking HAL (and util).  
  - Targets: `all`, `clean`. Optional: `install` (e.g. PREFIX=/usr/local).  
  - CFLAGS: `-I include`, `-std=c11`, warnings. LDFLAGS: `-lgpiod` (and `-lrt` if using clock_gettime).  
  - Document in server README or root: build on device (`make`) and optional cross-compile (e.g. set CC for ARM).

## 5. Definition of Done (from M0)

- Wiring documented in `docs/ads1278_wiring.md` (pin table + GPIO lines).
- `ads1278_dump` runs on RP OS 2.07; captures **≥ 10,000 consecutive frames** without alignment failure.
- Produces stable, plausible codes for a known input (e.g. grounded inputs, or known voltage).
- /SYNC supported when wired; settling frames discarded after SYNC pulse.
- Output format documented in `docs/ads1278_output.md` (channel order, sign extension, timestamps, binary layout).

## 6. M1 hook (no implementation in M0)

Keep the HAL as the only ADS1278 interface. Later, M2 will feed `ads1278_read_frame()` into the acq/ ring buffer and net/ protocol; `ads1278_dump` remains a standalone diagnostic tool.

## Order of implementation

1. **HAL**: Create `server/include/ads1278.h` and `server/src/spi/ads1278/ads1278.c` (SPI init, 24-byte transfer, parse; GPIO DRDY/SYNC with libgpiod; open/start/read_frame/stop/close).
2. **Tool**: Create `server/tools/ads1278_dump.c` (CLI, loop, --out binary write, --print, timeout/overrun handling).
3. **Build**: Add `server/Makefile` and ensure it builds on a Linux host (and document RP/cross-compile).
4. **Docs**: Add `docs/ads1278_wiring.md` (template + fill after wiring) and `docs/ads1278_output.md` (format spec).
5. **Validate**: Run on RP with EVM wired; capture 10k frames, verify no timeouts and plausible values; optionally raw hex dump for first frames.

