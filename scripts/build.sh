#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

cmake -B build/ --install-prefix ~/.local -DBUILD_TESTING=ON "$@"
cmake --build build/
