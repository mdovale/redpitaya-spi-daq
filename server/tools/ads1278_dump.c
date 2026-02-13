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
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t gpio_number;
    bool set;
} gpio_endpoint_t;

static void usage(FILE *stream, const char *prog_name)
{
    fprintf(stream,
        "Usage: %s [options]\n"
        "\n"
        "Required:\n"
        "  --drdy <gpio_number>                  DRDY input GPIO number\n"
        "\n"
        "Optional:\n"
        "  --spidev <path>                      SPI device (default: %s)\n"
        "  --sclk-hz <hz>                       SPI clock (default: 1000000)\n"
        "  --spi-mode <0..3>                    SPI mode (default: 0)\n"
        "  --sync <gpio_number>                  SYNC output GPIO number\n"
        "  --no-sync                            Disable SYNC pulse\n"
        "  --settle-frames <n>                  Discard N frames after SYNC pulse\n"
        "  --drdy-timeout-ms <ms>               DRDY wait timeout (default: %u)\n"
        "  --frames <n>                         Frames to capture (default: 1000)\n"
        "  --out <path>                         Write binary capture records\n"
        "  --print                              Pretty-print each frame\n"
        "  --hex <n>                            Hex dump first N raw SPI frames\n"
        "  --help                               Show this help text\n"
        "\n"
        "Notes:\n"
        "  - This build uses sysfs GPIO only.\n"
        "  - Pass global GPIO numbers (e.g. 968, 969 from /sys/kernel/debug/gpio).\n",
        prog_name,
        ADS1278_DEFAULT_SPIDEV,
        ADS1278_DEFAULT_DRDY_TIMEOUT_MS);
}

static int parse_u32(const char *text, uint32_t *out_value)
{
    char *end = NULL;
    unsigned long value;

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > UINT32_MAX) {
        return -1;
    }

    *out_value = (uint32_t)value;
    return 0;
}

static int parse_u64(const char *text, uint64_t *out_value)
{
    char *end = NULL;
    unsigned long long value;

    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return -1;
    }

    *out_value = (uint64_t)value;
    return 0;
}

static int parse_gpio_endpoint(const char *text, gpio_endpoint_t *out_endpoint)
{
    const char *sep = strchr(text, ':');

    if (sep == NULL) {
        uint32_t gpio_number;

        if (parse_u32(text, &gpio_number) != 0) {
            return -1;
        }

        out_endpoint->gpio_number = gpio_number;
        out_endpoint->set = true;
        return 0;
    }

    if (strncmp(text, "sysfs:", 6) != 0) {
        return -1;
    }

    {
        uint32_t gpio_number = 0U;

        if (parse_u32(sep + 1, &gpio_number) != 0) {
            return -1;
        }

        out_endpoint->gpio_number = gpio_number;
        out_endpoint->set = true;
        return 0;
    }
}

static void free_gpio_endpoint(gpio_endpoint_t *endpoint)
{
    endpoint->gpio_number = 0U;
    endpoint->set = false;
}

static int write_u64_le(FILE *out_file, uint64_t value)
{
    uint8_t bytes[8];
    size_t idx;

    for (idx = 0; idx < 8U; ++idx) {
        bytes[idx] = (uint8_t)((value >> (idx * 8U)) & 0xFFU);
    }

    return (fwrite(bytes, 1U, sizeof(bytes), out_file) == sizeof(bytes)) ? 0 : -1;
}

static int write_i32_le(FILE *out_file, int32_t value)
{
    uint32_t cast_value = (uint32_t)value;
    uint8_t bytes[4];
    size_t idx;

    for (idx = 0; idx < 4U; ++idx) {
        bytes[idx] = (uint8_t)((cast_value >> (idx * 8U)) & 0xFFU);
    }

    return (fwrite(bytes, 1U, sizeof(bytes), out_file) == sizeof(bytes)) ? 0 : -1;
}

static int write_frame_record(FILE *out_file, const ads1278_frame_t *frame)
{
    uint32_t idx;

    if (write_u64_le(out_file, frame->seq) != 0) {
        return -1;
    }
    if (write_u64_le(out_file, frame->tstamp_ns) != 0) {
        return -1;
    }
    for (idx = 0; idx < ADS1278_CHANNEL_COUNT; ++idx) {
        if (write_i32_le(out_file, frame->ch[idx]) != 0) {
            return -1;
        }
    }

    return 0;
}

static void print_frame(const ads1278_frame_t *frame)
{
    uint32_t idx;

    printf("seq=%" PRIu64 " tstamp_ns=%" PRIu64 " ch=[",
        frame->seq, frame->tstamp_ns);
    for (idx = 0; idx < ADS1278_CHANNEL_COUNT; ++idx) {
        printf("%" PRId32 "%s", frame->ch[idx], (idx + 1U == ADS1278_CHANNEL_COUNT) ? "" : ", ");
    }
    printf("]\n");
}

static void print_raw_hex(const uint8_t raw[ADS1278_TDM_FRAME_BYTES], uint64_t seq)
{
    uint32_t idx;

    printf("raw seq=%" PRIu64 ":", seq);
    for (idx = 0; idx < ADS1278_TDM_FRAME_BYTES; ++idx) {
        printf(" %02X", raw[idx]);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    const char *spidev_path = ADS1278_DEFAULT_SPIDEV;
    uint32_t sclk_hz = 1000000U;
    uint32_t spi_mode = 0U;
    uint32_t settle_frames = 0U;
    uint32_t drdy_timeout_ms = ADS1278_DEFAULT_DRDY_TIMEOUT_MS;
    uint32_t hex_frames = 0U;
    uint64_t frames_to_capture = 1000U;
    bool pretty_print = false;
    bool use_sync = true;
    const char *out_path = NULL;
    FILE *out_file = NULL;
    gpio_endpoint_t drdy = {0};
    gpio_endpoint_t sync = {0};
    uint64_t captured = 0U;
    int exit_code = EXIT_FAILURE;

    static const struct option long_options[] = {
        {"spidev", required_argument, NULL, 'd'},
        {"sclk-hz", required_argument, NULL, 's'},
        {"spi-mode", required_argument, NULL, 'm'},
        {"drdy", required_argument, NULL, 'r'},
        {"sync", required_argument, NULL, 'y'},
        {"no-sync", no_argument, NULL, 'n'},
        {"settle-frames", required_argument, NULL, 't'},
        {"drdy-timeout-ms", required_argument, NULL, 'w'},
        {"frames", required_argument, NULL, 'f'},
        {"out", required_argument, NULL, 'o'},
        {"print", no_argument, NULL, 'p'},
        {"hex", required_argument, NULL, 'x'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    while (1) {
        int opt = getopt_long(argc, argv, "d:s:m:r:y:nt:w:f:o:px:h", long_options, NULL);
        if (opt == -1) {
            break;
        }

        switch (opt) {
            case 'd':
                spidev_path = optarg;
                break;
            case 's':
                if (parse_u32(optarg, &sclk_hz) != 0) {
                    fprintf(stderr, "Invalid --sclk-hz: %s\n", optarg);
                    goto cleanup;
                }
                break;
            case 'm':
                if (parse_u32(optarg, &spi_mode) != 0 || spi_mode > 3U) {
                    fprintf(stderr, "Invalid --spi-mode: %s\n", optarg);
                    goto cleanup;
                }
                break;
            case 'r':
                free_gpio_endpoint(&drdy);
                if (parse_gpio_endpoint(optarg, &drdy) != 0) {
                    fprintf(stderr, "Invalid --drdy: %s\n", optarg);
                    goto cleanup;
                }
                break;
            case 'y':
                free_gpio_endpoint(&sync);
                if (parse_gpio_endpoint(optarg, &sync) != 0) {
                    fprintf(stderr, "Invalid --sync: %s\n", optarg);
                    goto cleanup;
                }
                break;
            case 'n':
                use_sync = false;
                break;
            case 't':
                if (parse_u32(optarg, &settle_frames) != 0) {
                    fprintf(stderr, "Invalid --settle-frames: %s\n", optarg);
                    goto cleanup;
                }
                break;
            case 'w':
                if (parse_u32(optarg, &drdy_timeout_ms) != 0) {
                    fprintf(stderr, "Invalid --drdy-timeout-ms: %s\n", optarg);
                    goto cleanup;
                }
                break;
            case 'f':
                if (parse_u64(optarg, &frames_to_capture) != 0 || frames_to_capture == 0U) {
                    fprintf(stderr, "Invalid --frames: %s\n", optarg);
                    goto cleanup;
                }
                break;
            case 'o':
                out_path = optarg;
                break;
            case 'p':
                pretty_print = true;
                break;
            case 'x':
                if (parse_u32(optarg, &hex_frames) != 0) {
                    fprintf(stderr, "Invalid --hex: %s\n", optarg);
                    goto cleanup;
                }
                break;
            case 'h':
                usage(stdout, argv[0]);
                exit_code = EXIT_SUCCESS;
                goto cleanup;
            default:
                usage(stderr, argv[0]);
                goto cleanup;
        }
    }

    if (!drdy.set) {
        fprintf(stderr, "--drdy is required.\n");
        usage(stderr, argv[0]);
        goto cleanup;
    }

    if (use_sync && !sync.set) {
        fprintf(stderr, "--sync is required unless --no-sync is used.\n");
        goto cleanup;
    }

    if (out_path != NULL) {
        out_file = fopen(out_path, "wb");
        if (out_file == NULL) {
            perror("fopen(--out)");
            goto cleanup;
        }
    }

    {
        ads1278_cfg_t cfg = {0};
        uint64_t idx;

        cfg.spidev_path = spidev_path;
        cfg.sclk_hz = sclk_hz;
        cfg.spi_mode = (uint8_t)spi_mode;
        cfg.spi_no_cs = true;
        cfg.drdy_gpio_number = drdy.gpio_number;
        cfg.use_sync = use_sync;
        cfg.sync_gpio_number = use_sync ? sync.gpio_number : 0U;
        cfg.settle_frames = settle_frames;
        cfg.drdy_timeout_ms = drdy_timeout_ms;

        if (ads1278_open(&cfg) != 0) {
            perror("ads1278_open");
            goto cleanup;
        }

        if (ads1278_start() != 0) {
            perror("ads1278_start");
            ads1278_close();
            goto cleanup;
        }

        for (idx = 0U; idx < frames_to_capture; ++idx) {
            ads1278_frame_t frame = {0};

            if (ads1278_read_frame(&frame) != 0) {
                perror("ads1278_read_frame");
                ads1278_stop();
                ads1278_close();
                goto cleanup;
            }

            ++captured;

            if (pretty_print) {
                print_frame(&frame);
            }

            if (idx < (uint64_t)hex_frames) {
                uint8_t raw[ADS1278_TDM_FRAME_BYTES];
                if (ads1278_get_last_raw_frame(raw) == 0) {
                    print_raw_hex(raw, frame.seq);
                }
            }

            if (out_file != NULL && write_frame_record(out_file, &frame) != 0) {
                perror("write_frame_record");
                ads1278_stop();
                ads1278_close();
                goto cleanup;
            }
        }

        ads1278_stop();
        ads1278_close();
    }

    if (out_file != NULL && fflush(out_file) != 0) {
        perror("fflush");
        goto cleanup;
    }

    fprintf(stderr, "Captured %" PRIu64 " frame(s).\n", captured);
    exit_code = EXIT_SUCCESS;

cleanup:
    if (out_file != NULL) {
        fclose(out_file);
    }
    free_gpio_endpoint(&drdy);
    free_gpio_endpoint(&sync);
    return exit_code;
}
