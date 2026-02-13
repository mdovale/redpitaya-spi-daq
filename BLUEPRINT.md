# BLUEPRINT — redpitaya-spi-daq (end-state)

This document describes the intended **end-state** of `redpitaya-spi-daq`: a Red Pitaya–hosted SPI acquisition server streaming ADC samples to a host-side Python client for live visualization and optional logging.

## Goals

- **Acquire** samples from an external **SPI ADC** using the Red Pitaya SPI interface.
- **Stream** samples off-board over the network with **low latency** and predictable throughput.
- **Visualize** the stream in real time from a cross-platform Python client.
- **Log/export** sample data for offline analysis.
- Keep the system **modular** so new ADCs and formats can be added without rewriting everything.

## Non-goals

- Implementing a fully general DAQ framework for all Red Pitaya peripherals.
- Building a “cloud” ingestion pipeline (keep it local/network-LAN oriented).

## High-level architecture

### Components

- **Server (`server/`)** — runs on the Red Pitaya (Linux):
  - Configures SPI and the attached ADC
  - Performs continuous sampling (or triggered/bursted sampling modes)
  - Packages data into a well-defined stream format
  - Serves data to one or more clients over TCP/IP

- **Client (`client/`)** — runs on a host machine:
  - Connects to the server over TCP/IP
  - Receives and parses the data stream
  - Displays real-time plots and basic controls
  - Optionally logs data to disk (binary + metadata) and can export to common formats

### Data flow (conceptual)

ADC → SPI (Red Pitaya) → server sampler/buffer → network streamer (TCP) → client receiver/buffer → plot/log

## Repository layout (target)

- `server/`
  - `src/`
    - `main.c` — entrypoint; arg parsing; lifecycle
    - `spi/` — SPI configuration, ADC driver(s)
    - `acq/` — sampling loop, DMA/PIO strategy, ring buffers
    - `net/` — TCP server, framing/protocol, client sessions
    - `util/` — logging, timing, config, endian helpers
  - `include/` — public headers
  - `tests/` — (where feasible) unit tests for framing/CRC/config parsing
  - `Makefile` or `CMakeLists.txt`
  - `README.md` (server-specific usage)

- `client/`
  - `src/`
    - `main.py` — app entrypoint (CLI) / GUI launcher
    - `protocol.py` — stream framing parser/serializer
    - `net.py` — connection management, reconnect, buffering
    - `ui/` — plotting widgets, controls (PySide6 + pyqtgraph)
    - `logging.py` — capture to disk, file formats
  - `pyproject.toml` (preferred) or `requirements.txt` — deps include **PySide6**, **pyqtgraph**
  - `README.md` (client-specific usage)

- Root
  - `README.md` — project overview
  - `BLUEPRINT.md` — this document
  - `LICENSE` — BSD-3 (recommended to add at root)
  - `docs/` — protocol spec, ADC notes, wiring diagrams, examples
  - `examples/` — minimal scripts for headless capture, playback, plotting

## Server: functional requirements

### SPI + ADC

- **ADC driver abstraction**
  - Support at least one target ADC initially (configurable SPI mode, word size, clocks)
  - Clean interface for additional ADCs later (register init, read sample(s), calibration hooks)

- **Sampling modes**
  - **Continuous** streaming mode (default)
  - Optional: **burst**/finite capture and **triggered** capture (future)

### Buffering and performance

- Use a **ring buffer** between acquisition and network streaming.
- Define measurable targets (tunable by build/hardware):
  - Sustained streaming without overruns at a practical sample rate
  - Bounded end-to-end latency under nominal load

### Network server behavior

- TCP server listens on a configurable host/port.
- Supports:
  - Single client initially; optionally multi-client (fan-out) later.
  - Graceful handling of disconnects and reconnects.
  - Backpressure strategy: drop-oldest, block-producer, or configurable policy.

### Configuration

Support configuration via:

- CLI flags (primary)
- Optional config file (TOML/YAML/INI) later

Parameters should include (at minimum):

- SPI device selection (e.g., `/dev/spidevX.Y`)
- SPI mode (CPOL/CPHA), bits-per-word, speed
- ADC-specific init parameters
- Sample rate / decimation (if applicable)
- Network listen address/port
- Stream format selection and metadata flags
- Logging level / diagnostics

### Observability

- Structured logs to stderr/syslog (severity levels).
- Runtime counters:
  - samples acquired / streamed
  - buffer occupancy high-water marks
  - overruns/underruns
  - client disconnects/reconnects

## Client: functional requirements

### GUI stack

- **PySide6**: Qt for Windows, widgets, and event loop. Use for the main application window and controls.
- **pyqtgraph**: Real-time plotting. Use for the live time-domain plot (not matplotlib). Integrate with PySide6 widgets.

### Receiver + buffering

- Robust stream parser with resynchronization (handles partial frames).
- Automatic reconnect with backoff.
- Optional: ability to request configuration/state from server.

### UI/UX

- Live plot (time-domain) with:
  - pause/resume
  - scaling/zoom
  - basic stats (min/max/RMS)
  - sample rate / effective rate display
- Controls for connecting to server and starting/stopping capture.

### Logging + export

- Write **capture files** containing:
  - raw samples
  - metadata (sample rate, format, start time, ADC config)
- Export to common formats (at least one):
  - CSV (simple but large)
  - NumPy `.npy`/`.npz` (recommended)

## Wire protocol (target)

Define and document a **versioned** stream protocol in `docs/protocol.md`.

### Principles

- **Framed** messages (not ad-hoc socket reads)
- **Self-describing**: includes version and payload type
- **Binary-first** for efficiency, with optional JSON control messages if needed

### Suggested minimal message types

- `HELLO` (server → client): protocol version, server build info
- `CONFIG` (server → client): active acquisition settings
- `DATA` (server → client): sample frames (timestamp + samples)
- `STATS` (server → client): periodic counters (optional)
- `SET_CONFIG` (client → server): request config changes (optional/future)

### DATA framing (example shape; to finalize)

- Header: magic, version, type, flags, sequence, payload_len, optional CRC
- Payload: timestamp + interleaved samples (e.g., int16/int24 packed/int32)

## Testing strategy (target)

- **Server**
  - Unit tests for protocol framing and config parsing (host-buildable)
  - Hardware-in-the-loop tests (documented) for SPI sampling correctness
- **Client**
  - Parser tests using recorded fixture streams
  - UI smoke test instructions (manual) and optional automated checks

## Build & run workflow (target)

### Server

- Build on Red Pitaya or cross-compile (document both).
- Provide a single command to run with sensible defaults, e.g.:
  - `./rp_spi_daq --spidev /dev/spidevX.Y --port 9000 --adc <name>`

### Client

- Install via `pip`/`uv` using `pyproject.toml`. Required dependencies: **PySide6**, **pyqtgraph** (plus numpy for capture/export as needed).
- Run via:
  - `python -m redpitaya_spi_daq_client --host <rp-ip> --port 9000`

## Documentation deliverables (target)

- `docs/wiring.md`: SPI wiring, voltage levels, grounding, shielding notes
- `docs/protocol.md`: full protocol spec + examples
- `docs/adc/<adc>.md`: ADC-specific initialization and timing requirements
- `examples/`: end-to-end scripts (headless capture, playback viewer)

## Milestones (suggested)

- **M0**: ADS1278 SPI bring-up in ARM userspace
- **M1**: Minimal server streams dummy ramp + client plots it (protocol + UI skeleton).
- **M2**: Real SPI reads for one ADC model + stable streaming to client.
- **M3**: Logging/export + robust reconnect + stats/health reporting.
- **M4**: Multiple formats / optional triggered capture + documentation polish.

## ADS1278 hardware layer

This project’s first external ADC target is the **TI ADS1278** (8-channel, 24-bit, simultaneous-sampling ΔΣ ADC) connected to the Red Pitaya via:

- **E2 SPI** for the serial data stream (DOUT1 → MISO, SCLK → SCLK)
- **GPIO** lines for **/DRDY** (data-ready) and **/SYNC** (synchronization / reset of the digital filter pipeline)

The goal of this “hardware layer” (HAL) is to turn “GPIO edges + SPI bytes” into **validated 8-channel sample frames** so that higher layers (buffering, networking, client plotting) remain independent of wiring, timing edges, and Linux device details.

### Key ADS1278 behaviors that shape the HAL

- **Simultaneous sampling**: all eight channels convert in parallel and are internally synchronized (no inter-channel sample skew from “round-robin” multiplexing).
- **SPI is read-only**: there is no register map to configure over SPI; the ADS1278 is configured using hardware pins (MODE, FORMAT, CLKDIV, PWDN…).
- **Framing is DRDY-driven**: /DRDY indicates when a coherent conversion frame is available; readout must be aligned to /DRDY to avoid mixing bits from different conversions.
- **24-bit two’s complement samples**: each channel is a signed 24-bit word; software must sign-extend to 32-bit.
- **TDM output is the practical ARM-only path**: in TDM mode, all channels are serialized on DOUT1 in a fixed order, producing a single frame per /DRDY. TDM (Time-Division Multiplexing) is a scheme where multiple channels share one data line by transmitting their samples sequentially in fixed time slots (e.g., CH1, then CH2, …, then repeat).

Primary references:
- ADS1278 product/datasheet landing: https://www.ti.com/product/ADS1278
- ADS1278 datasheet PDF (EP version): https://www.ti.com/lit/gpn/ADS1278-EP
- ADS1278EVM user guide: https://www.ti.com/lit/pdf/sbau436

### Recommended data format for ARM-only bring-up (M0)

Use **SPI + TDM + Fixed-position** output format so the ARM can read all 8 channels over a single MISO line in a deterministic layout:

- Frame size: 8 channels × 24 bits = **192 bits = 24 bytes**
- Channel order in TDM: **CH1, CH2, …, CH8** on DOUT1 (then repeats each conversion)

This keeps parsing stable even if you later power down channels (fixed-position formats keep slots consistent).

### Clock-rate strategy (critical for “few Hz” use, and for Linux feasibility)

The ADS1278 output data rate scales with its master clock frequency (down to a stated minimum), and TI notes that selecting a slower external clock does **not** change conversion resolution; it reduces throughput and can reduce external clock-buffer power.

Practical strategy:
1. Start with the EVM’s default clock configuration to reduce variables.
2. If /DRDY is too fast for reliable userspace servicing, switch the EVM to an external clock and **reduce fCLK** until the ARM can service every /DRDY edge comfortably.

### Physical wiring (Red Pitaya ↔ ADS1278EVM)

#### ADS1278EVM: external controller header (J6)

The ADS1278EVM user guide supports connecting an external controller via J6 and recommends:
- Tie **DIN** low if not daisy-chaining (do not leave DIN floating)
- Connect controller I/O to **/SYNC**
- Connect controller input to **/DRDY_FSYNC**
- Use **DOUT1** for TDM output

#### Red Pitaya PRO Gen 2: E2 SPI pins

On the STEMlab 125-14 PRO Gen 2 E2 connector:
- Pin 3: SPI (MOSI)  — PS_MIO10 — 3.3V
- Pin 4: SPI (MISO)  — PS_MIO11 — 3.3V
- Pin 5: SPI (SCK)   — PS_MIO12 — 3.3V
- Pin 6: SPI (CS)    — PS_MIO13 — 3.3V

Wire for TDM readout:
- E2 SCK  → EVM SCLK
- E2 MISO ← EVM DOUT1 (TDM stream)

DIN handling (choose one):
- Preferred (matches EVM guidance): strap **DIN to GND** on the EVM and leave MOSI unconnected.
- Acceptable alternative: connect MOSI→DIN and always transmit 0x00 so DIN is never floating.

CS handling:
- ADS1278 SPI readout does not require a chip-select pin. Leave E2 CS unconnected and configure the SPI driver accordingly (or tolerate toggling on an unconnected line).

#### /DRDY and /SYNC

Add two GPIOs:
- EVM /DRDY_FSYNC → RP GPIO input (falling-edge event)
- EVM /SYNC       → RP GPIO output (pulse low/high on startup and for re-sync)

Use sysfs GPIO numbers for `/DRDY` and `/SYNC` in this codebase baseline. Document exact GPIO numbers used in `docs/ads1278_wiring.md` for reproducibility.

### Linux device interfaces (M0 baseline)

SPI:
- Gen 2 boards use **`/dev/spidev2.0`** (not the classic `/dev/spidev1.0`).

GPIO:
- Use sysfs GPIO (`/sys/class/gpio`) for `/DRDY` edge wait and `/SYNC` output control.

Red Pitaya reference:
- SPI command docs note the Gen 2 device path difference: https://redpitaya.readthedocs.io/en/latest/appsFeatures/remoteControl/command_list/commands-spi.html

### HAL responsibilities

The ADS1278 HAL must:

1. Open and configure SPI (mode, speed, bits-per-word)
2. Configure GPIO:
   - /DRDY as input with falling-edge events
   - /SYNC as output (optional but recommended)
3. Optional: pulse /SYNC after power-up for deterministic alignment
4. For each conversion:
   - wait for /DRDY falling edge
   - perform exactly one SPI transfer of **24 bytes**
   - parse into 8 × signed 32-bit values (sign-extended from 24-bit)
5. Report/track:
   - timeouts (no /DRDY)
   - overruns (transfer too slow vs conversion period)
   - basic framing sanity checks (optional raw hex dump)

This HAL must *not* attempt to configure ADS1278 operating modes via SPI (no register map). Mode changes are done via EVM straps/jumpers and/or external clock selection.

### Proposed HAL API (C)

Location suggestion:
- `server/src/spi/ads1278/ads1278.c`
- `server/include/ads1278.h`

```c
typedef struct {
    const char *spidev_path;     // e.g. "/dev/spidev2.0"
    uint32_t    sclk_hz;         // SPI clock rate
    uint8_t     spi_mode;        // default 0
    bool        spi_no_cs;       // true (ADS1278 has no CS pin)
    // DRDY
    uint32_t    drdy_gpio_number; // sysfs global GPIO number
    // SYNC (optional)
    uint32_t    sync_gpio_number;
    bool        use_sync;        // recommended true for deterministic startup
    uint32_t    settle_frames;   // discard N frames after SYNC (datasheet/empirical)
} ads1278_cfg_t;

typedef struct {
    uint64_t seq;
    uint64_t tstamp_ns;          // CLOCK_MONOTONIC(_RAW) timestamp
    int32_t  ch[8];              // sign-extended 24-bit samples
} ads1278_frame_t;

int  ads1278_open(const ads1278_cfg_t *cfg);
int  ads1278_start(void);
int  ads1278_read_frame(ads1278_frame_t *out);  // blocks until DRDY then reads SPI
void ads1278_stop(void);
void ads1278_close(void);
```

### Integration points

- `spi/ads1278` produces `ads1278_frame_t`
- `acq/` owns buffering + decimation/averaging for low-frequency use
- `net/` owns protocol serialization
- `client/` remains unchanged once it can plot frames
