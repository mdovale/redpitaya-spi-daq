# BSD 3-Clause License
#
# Copyright (c) 2026, Miguel Dovale (University of Arizona)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

CC ?= cc
BUILD_DIR ?= build
USE_LIBGPIOD ?= 1

CPPFLAGS ?=
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?=

CPPFLAGS += -Iinclude -D_POSIX_C_SOURCE=200809L -DADS1278_USE_LIBGPIOD=$(USE_LIBGPIOD)

ifeq ($(USE_LIBGPIOD),1)
LDLIBS += -lgpiod
endif

HAL_SRC := src/spi/ads1278/ads1278.c
HAL_OBJ := $(BUILD_DIR)/$(HAL_SRC:.c=.o)
HAL_LIB := $(BUILD_DIR)/libads1278.a

TOOL_SRC := tools/ads1278_dump.c
TOOL_OBJ := $(BUILD_DIR)/$(TOOL_SRC:.c=.o)
TOOL_BIN := ads1278_dump

.PHONY: all clean

all: $(TOOL_BIN)

$(TOOL_BIN): $(TOOL_OBJ) $(HAL_LIB)
	$(CC) $(LDFLAGS) -o $@ $(TOOL_OBJ) $(HAL_LIB) $(LDLIBS)

$(HAL_LIB): $(HAL_OBJ)
	@mkdir -p "$(dir $@)"
	ar rcs $@ $^

$(BUILD_DIR)/%.o: %.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf "$(BUILD_DIR)" "$(TOOL_BIN)"
