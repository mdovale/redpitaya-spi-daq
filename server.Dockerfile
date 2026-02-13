# Red Pitaya server cross-build: ARM 32-bit (armhf) on Ubuntu 22.04
# Use with server-build-docker.sh to produce RedPitaya-ready executables from any host.

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    file \
    gcc-arm-linux-gnueabihf \
    g++-arm-linux-gnueabihf \
    make \
    pkg-config \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /work
