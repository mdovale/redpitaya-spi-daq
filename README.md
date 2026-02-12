# redpitaya-spi-daq

Data acquisition (DAQ) for **Red Pitaya** using an external **SPI ADC**, split into:

- **`server/`**: C program intended to run on the Red Pitaya (SPI acquisition + network streaming)
- **`client/`**: Python client intended to run on a host machine (receive + visualize/log samples)

## Status

This repository is currently a **scaffold**: `server/main.c` and `client/main.py` contain licensing headers, but the DAQ/streaming/GUI implementation is not yet present.

## Design overview

The goal is to keep acquisition close to the hardware (on the Red Pitaya) while keeping visualization and analysis on a regular computer.

SPI is a common four-wire synchronous protocol (SCLK, MOSI, MISO, CS) used for high-speed peripherals like ADCs, DACs, and sensors.

## Repository layout

- **`server/`**: embedded/Linux-side C code (SPI + network server)
- **`client/`**: Python application (network client + plotting/controls)

## Planned features

- **Real-time SPI ADC readout**: configure + sample an external ADC via Red Pitaya SPI
- **Network streaming**: forward samples over TCP/IP for low-latency clients
- **Cross-platform client**: live plotting plus optional logging/exports

## References

- [Red Pitaya API scripts (remote control)](https://redpitaya.readthedocs.io/en/latest/appsFeatures/remoteControl/API_scripts.html)
- [Red Pitaya SPI HW API example](https://redpitaya.readthedocs.io/en/latest/appsFeatures/examples/communication_interfaces/dig_com-6-spi_hw_api.html)
- [Red Pitaya Zynq SPI notes (rfblocks.org)](https://rfblocks.org/articles/RedPitaya-zynq-spi.html)
- [Red Pitaya C API reference](https://github.com/RedPitaya/RedPitaya/tree/master/rp-api)
- [Red Pitaya C API rp.h](https://github.com/RedPitaya/RedPitaya/blob/master/rp-api/api/include/rp.h)
- [Red Pitaya C API rp_hw.h](https://github.com/RedPitaya/RedPitaya/blob/master/rp-api/api-hw/include/rp_hw.h)