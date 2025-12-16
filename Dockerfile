FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    zip \
    unzip \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt
RUN git clone https://github.com/microsoft/vcpkg.git \
    && ./vcpkg/bootstrap-vcpkg.sh

ENV VCPKG_ROOT=/opt/vcpkg

WORKDIR /app

COPY . .

RUN mkdir build && \
    cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake && \
    cmake --build build --config Release

