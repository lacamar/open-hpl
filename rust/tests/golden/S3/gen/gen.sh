#!/usr/bin/env bash
# Usage: gen.sh <outbin>; then <outbin> 12 120 > ../s3.txt
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
c++ -std=c++17 -O1 -mcpu=apple-m2+crc+aes+sha3+fp16 "$here/gen.cpp" -o "$1"
