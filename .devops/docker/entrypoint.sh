#!/usr/bin/env bash

set -o errexit
set -o pipefail
set -o nounset


VCPKG_CMAKE_PATH="${VCPKG_CMAKE_PATH:-/opt/vcpkg/scripts/buildsystems/vcpkg.cmake}"


if [ "${LOGPASS_ENV}" != 'production' ];
then
    cd /app \
    && cmake \
        -B build \
        -D CMAKE_TOOLCHAIN_FILE="${VCPKG_CMAKE_PATH}" \
        -G "Ninja Multi-Config" \
        && time cmake \
            --build build \
            --config Debug \
            --parallel \
    ; cd -
fi

ulimit -n 65535
ulimit -c unlimited
cmd="$*"
$cmd  # evaluate passed command
