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

- **M1**: Minimal server streams dummy ramp + client plots it (protocol + UI skeleton).
- **M2**: Real SPI reads for one ADC model + stable streaming to client.
- **M3**: Logging/export + robust reconnect + stats/health reporting.
- **M4**: Multiple formats / optional triggered capture + documentation polish.
