#!/usr/bin/env bash
set -euo pipefail

version="${MONGOOSE_VERSION:-7.17}"
base="https://raw.githubusercontent.com/cesanta/mongoose/${version}"
if [[ ! -s app/mongoose.c ]]; then
  curl --fail --silent --show-error --location "$base/mongoose.c" -o app/mongoose.c
fi
if [[ ! -s app/mongoose.h ]]; then
  curl --fail --silent --show-error --location "$base/mongoose.h" -o app/mongoose.h
fi
