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

#include "ads1278.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef __linux__

int ads1278_open(const ads1278_cfg_t *cfg)
{
    (void)cfg;
    errno = ENOTSUP;
    return -1;
}

int ads1278_start(void)
{
    errno = ENOTSUP;
    return -1;
}

int ads1278_read_frame(ads1278_frame_t *out)
{
    (void)out;
    errno = ENOTSUP;
    return -1;
}

int ads1278_get_last_raw_frame(uint8_t out[ADS1278_TDM_FRAME_BYTES])
{
    (void)out;
    errno = ENOTSUP;
    return -1;
}

void ads1278_stop(void)
{
}

void ads1278_close(void)
{
}

#else

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define ADS1278_SYNC_PULSE_US 10U
#define ADS1278_OVERLONG_XFER_WARN_US 5000U

typedef struct {
    uint32_t line_number;
    int fd;
    int exported;
} ads1278_gpio_t;

typedef struct {
    int is_open;
    int started;
    int spi_fd;
    uint64_t seq;
    ads1278_cfg_t cfg;
    ads1278_gpio_t drdy_gpio;
    ads1278_gpio_t sync_gpio;
    uint8_t tx_zeros[ADS1278_TDM_FRAME_BYTES];
    uint8_t last_raw[ADS1278_TDM_FRAME_BYTES];
} ads1278_ctx_t;

static ads1278_ctx_t g_ctx = {
    .spi_fd = -1,
    .drdy_gpio = {.fd = -1},
    .sync_gpio = {.fd = -1}
};

static int write_text_file(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    ssize_t written;

    if (fd < 0) {
        return -1;
    }

    written = write(fd, value, strlen(value));
    if (written < 0 || (size_t)written != strlen(value)) {
        int saved_errno = (written < 0) ? errno : EIO;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    if (close(fd) < 0) {
        return -1;
    }

    return 0;
}

static int sysfs_export_gpio(uint32_t line_number, int *did_export)
{
    char buf[32];
    int rc;

    snprintf(buf, sizeof(buf), "%u", line_number);
    rc = write_text_file("/sys/class/gpio/export", buf);
    if (rc == 0) {
        *did_export = 1;
        return 0;
    }

    if (errno == EBUSY) {
        *did_export = 0;
        return 0;
    }
    return -1;
}

static int sysfs_unexport_gpio(uint32_t line_number)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "%u", line_number);
    return write_text_file("/sys/class/gpio/unexport", buf);
}

static int sysfs_set_gpio_attr(uint32_t line_number, const char *attr, const char *value)
{
    char path[96];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/%s", line_number, attr);
    return write_text_file(path, value);
}

static int sysfs_open_gpio_value(uint32_t line_number, int flags)
{
    char path[96];

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/value", line_number);
    return open(path, flags | O_CLOEXEC);
}

static uint64_t monotonic_now_ns(void)
{
    struct timespec ts = {0, 0};

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }

    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

static void gpio_reset(ads1278_gpio_t *gpio)
{
    memset(gpio, 0, sizeof(*gpio));
    gpio->fd = -1;
}

static int gpio_open_drdy(ads1278_gpio_t *gpio, uint32_t line_number)
{
    gpio_reset(gpio);
    gpio->line_number = line_number;

    if (sysfs_export_gpio(line_number, &gpio->exported) != 0) {
        return -1;
    }

    if (sysfs_set_gpio_attr(line_number, "direction", "in") != 0) {
        return -1;
    }

    if (sysfs_set_gpio_attr(line_number, "edge", "falling") != 0) {
        return -1;
    }

    gpio->fd = sysfs_open_gpio_value(line_number, O_RDONLY | O_NONBLOCK);
    if (gpio->fd < 0) {
        return -1;
    }

    return 0;
}

static int gpio_open_sync(ads1278_gpio_t *gpio, uint32_t line_number)
{
    gpio_reset(gpio);
    gpio->line_number = line_number;

    if (sysfs_export_gpio(line_number, &gpio->exported) != 0) {
        return -1;
    }

    if (sysfs_set_gpio_attr(line_number, "direction", "out") != 0) {
        return -1;
    }

    if (sysfs_set_gpio_attr(line_number, "value", "1") != 0) {
        return -1;
    }

    gpio->fd = sysfs_open_gpio_value(line_number, O_RDWR);
    if (gpio->fd < 0) {
        return -1;
    }

    return 0;
}

static int gpio_set_value(const ads1278_gpio_t *gpio, int value)
{
    const char out = (value == 0) ? '0' : '1';

    if (gpio->fd < 0) {
        errno = EBADF;
        return -1;
    }

    if (lseek(gpio->fd, 0, SEEK_SET) < 0) {
        return -1;
    }

    if (write(gpio->fd, &out, 1) != 1) {
        if (errno == 0) {
            errno = EIO;
        }
        return -1;
    }

    return 0;
}

static int gpio_wait_drdy_event(const ads1278_gpio_t *gpio, uint32_t timeout_ms)
{
    char junk[8];
    struct pollfd pfd = {0};
    int rc;

    if (gpio->fd < 0) {
        errno = EBADF;
        return -1;
    }

    if (lseek(gpio->fd, 0, SEEK_SET) < 0) {
        return -1;
    }
    (void)read(gpio->fd, junk, sizeof(junk));

    pfd.fd = gpio->fd;
    pfd.events = POLLPRI | POLLERR;
    rc = poll(&pfd, 1, (int)timeout_ms);
    if (rc < 0) {
        return -1;
    }
    if (rc == 0) {
        errno = ETIMEDOUT;
        return -1;
    }

    if (lseek(gpio->fd, 0, SEEK_SET) < 0) {
        return -1;
    }
    (void)read(gpio->fd, junk, sizeof(junk));

    return 0;
}

static void gpio_close(ads1278_gpio_t *gpio)
{
    if (gpio == NULL) {
        return;
    }

    if (gpio->fd >= 0) {
        close(gpio->fd);
    }
    if (gpio->exported) {
        (void)sysfs_unexport_gpio(gpio->line_number);
    }

    gpio_reset(gpio);
}

static int spi_open_and_configure(const ads1278_cfg_t *cfg)
{
    int fd = -1;
    uint8_t mode;
    uint8_t bits_per_word = 8U;
    uint32_t max_speed_hz;

    fd = open(cfg->spidev_path, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }

    mode = cfg->spi_mode;
#ifdef SPI_NO_CS
    if (cfg->spi_no_cs) {
        mode = (uint8_t)(mode | SPI_NO_CS);
    }
#endif

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        /* Some kernels/drivers reject SPI_NO_CS with EINVAL even when defined. */
#ifdef SPI_NO_CS
        if (cfg->spi_no_cs && errno == EINVAL) {
            mode = cfg->spi_mode;
            if (ioctl(fd, SPI_IOC_WR_MODE, &mode) == 0) {
                goto spi_mode_ok;
            }
        }
#endif
        close(fd);
        return -1;
    }

spi_mode_ok:
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word) < 0) {
        close(fd);
        return -1;
    }

    max_speed_hz = cfg->sclk_hz;
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &max_speed_hz) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int spi_read_24_bytes(uint8_t rx[ADS1278_TDM_FRAME_BYTES])
{
    struct spi_ioc_transfer transfer = {0};

    transfer.tx_buf = (uintptr_t)g_ctx.tx_zeros;
    transfer.rx_buf = (uintptr_t)rx;
    transfer.len = ADS1278_TDM_FRAME_BYTES;
    transfer.speed_hz = g_ctx.cfg.sclk_hz;
    transfer.bits_per_word = 8U;

    if (ioctl(g_ctx.spi_fd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        return -1;
    }

    return 0;
}

static void parse_samples_msb_first(const uint8_t raw[ADS1278_TDM_FRAME_BYTES], ads1278_frame_t *out)
{
    uint32_t idx;

    for (idx = 0; idx < ADS1278_CHANNEL_COUNT; ++idx) {
        uint32_t offset = idx * 3U;
        uint32_t raw24 = ((uint32_t)raw[offset] << 16U) |
            ((uint32_t)raw[offset + 1U] << 8U) |
            (uint32_t)raw[offset + 2U];

        if ((raw24 & 0x800000U) != 0U) {
            raw24 |= 0xFF000000U;
        }

        out->ch[idx] = (int32_t)raw24;
    }
}

int ads1278_open(const ads1278_cfg_t *cfg)
{
    ads1278_cfg_t effective_cfg;

    if (cfg == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (g_ctx.is_open) {
        errno = EALREADY;
        return -1;
    }

    memset(&effective_cfg, 0, sizeof(effective_cfg));
    effective_cfg = *cfg;
    if (effective_cfg.spidev_path == NULL) {
        effective_cfg.spidev_path = ADS1278_DEFAULT_SPIDEV;
    }
    if (effective_cfg.sclk_hz == 0U) {
        effective_cfg.sclk_hz = 1000000U;
    }
    if (effective_cfg.drdy_timeout_ms == 0U) {
        effective_cfg.drdy_timeout_ms = ADS1278_DEFAULT_DRDY_TIMEOUT_MS;
    }

    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.spi_fd = -1;
    gpio_reset(&g_ctx.drdy_gpio);
    gpio_reset(&g_ctx.sync_gpio);

    g_ctx.cfg = effective_cfg;
    g_ctx.spi_fd = spi_open_and_configure(&g_ctx.cfg);
    if (g_ctx.spi_fd < 0) {
        ads1278_close();
        return -1;
    }

    if (gpio_open_drdy(&g_ctx.drdy_gpio, g_ctx.cfg.drdy_gpio_number) != 0) {
        ads1278_close();
        return -1;
    }

    if (g_ctx.cfg.use_sync &&
        gpio_open_sync(&g_ctx.sync_gpio, g_ctx.cfg.sync_gpio_number) != 0) {
        ads1278_close();
        return -1;
    }

    g_ctx.seq = 0U;
    g_ctx.is_open = 1;
    return 0;
}

int ads1278_start(void)
{
    if (!g_ctx.is_open) {
        errno = ENODEV;
        return -1;
    }
    if (g_ctx.started) {
        return 0;
    }

    g_ctx.started = 1;
    if (g_ctx.cfg.use_sync) {
        uint32_t idx;
        ads1278_frame_t discard = {0};

        if (gpio_set_value(&g_ctx.sync_gpio, 0) != 0) {
            g_ctx.started = 0;
            return -1;
        }
        usleep(ADS1278_SYNC_PULSE_US);
        if (gpio_set_value(&g_ctx.sync_gpio, 1) != 0) {
            g_ctx.started = 0;
            return -1;
        }

        for (idx = 0; idx < g_ctx.cfg.settle_frames; ++idx) {
            if (ads1278_read_frame(&discard) != 0) {
                g_ctx.started = 0;
                return -1;
            }
        }
    }

    return 0;
}

int ads1278_read_frame(ads1278_frame_t *out)
{
    uint8_t raw[ADS1278_TDM_FRAME_BYTES] = {0};
    uint64_t drdy_ts_ns;
    uint64_t post_xfer_ns;

    if (out == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!g_ctx.is_open || !g_ctx.started) {
        errno = EPERM;
        return -1;
    }

    if (gpio_wait_drdy_event(&g_ctx.drdy_gpio, g_ctx.cfg.drdy_timeout_ms) != 0) {
        return -1;
    }

    drdy_ts_ns = monotonic_now_ns();
    if (spi_read_24_bytes(raw) != 0) {
        return -1;
    }
    post_xfer_ns = monotonic_now_ns();

    memcpy(g_ctx.last_raw, raw, sizeof(raw));

    out->seq = g_ctx.seq++;
    out->tstamp_ns = drdy_ts_ns;
    parse_samples_msb_first(raw, out);

    if (drdy_ts_ns != 0U && post_xfer_ns > drdy_ts_ns) {
        uint64_t elapsed_us = (post_xfer_ns - drdy_ts_ns) / 1000U;

        if (elapsed_us > ADS1278_OVERLONG_XFER_WARN_US) {
            fprintf(stderr,
                "ads1278 warning: slow transfer (%llu us), overrun risk\n",
                (unsigned long long)elapsed_us);
        }
    }

    return 0;
}

int ads1278_get_last_raw_frame(uint8_t out[ADS1278_TDM_FRAME_BYTES])
{
    if (out == NULL) {
        errno = EINVAL;
        return -1;
    }

    memcpy(out, g_ctx.last_raw, ADS1278_TDM_FRAME_BYTES);
    return 0;
}

void ads1278_stop(void)
{
    g_ctx.started = 0;
}

void ads1278_close(void)
{
    if (g_ctx.spi_fd >= 0) {
        close(g_ctx.spi_fd);
        g_ctx.spi_fd = -1;
    }

    gpio_close(&g_ctx.drdy_gpio);
    gpio_close(&g_ctx.sync_gpio);

    memset(g_ctx.last_raw, 0, sizeof(g_ctx.last_raw));
    g_ctx.seq = 0;
    g_ctx.started = 0;
    g_ctx.is_open = 0;
}

#endif /* __linux__ */
