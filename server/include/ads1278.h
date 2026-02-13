/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Miguel Dovale (University of Arizona)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ADS1278_H
#define ADS1278_H

#include <stdbool.h>
#include <stdint.h>

#define ADS1278_CHANNEL_COUNT 8U
#define ADS1278_TDM_FRAME_BYTES 24U
#define ADS1278_DEFAULT_SPIDEV "/dev/spidev2.0"
#define ADS1278_DEFAULT_DRDY_TIMEOUT_MS 2000U

typedef struct {
    const char *spidev_path;    /* e.g. "/dev/spidev2.0" */
    uint32_t sclk_hz;           /* SPI clock rate */
    uint8_t spi_mode;           /* default 0 */
    bool spi_no_cs;             /* true (ADS1278 has no CS pin) */

    /* DRDY (sysfs global GPIO number) */
    uint32_t drdy_gpio_number;

    /* SYNC (optional, sysfs global GPIO number) */
    uint32_t sync_gpio_number;
    bool use_sync;              /* recommended true for deterministic startup */
    uint32_t settle_frames;     /* discard N frames after SYNC pulse */

    /* Optional guard to avoid indefinite waits. */
    uint32_t drdy_timeout_ms;
} ads1278_cfg_t;

typedef struct {
    uint64_t seq;
    uint64_t tstamp_ns;         /* CLOCK_MONOTONIC timestamp */
    int32_t ch[ADS1278_CHANNEL_COUNT];
} ads1278_frame_t;

int ads1278_open(const ads1278_cfg_t *cfg);
int ads1278_start(void);
int ads1278_read_frame(ads1278_frame_t *out);
int ads1278_get_last_raw_frame(uint8_t out[ADS1278_TDM_FRAME_BYTES]);
void ads1278_stop(void);
void ads1278_close(void);

#endif /* ADS1278_H */
