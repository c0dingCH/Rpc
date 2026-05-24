#!/bin/bash
set -euo pipefail

THREADS=16
REQUESTS=40000
BUILD_DIR="$(cd "$(dirname "$0")/../build/test" && pwd)"

usage() {
  echo "Usage: $0 [--threads N] [--requests N]"
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --threads)   THREADS="$2";   shift 2 ;;
    --requests)  REQUESTS="$2";  shift 2 ;;
    *)           usage ;;
  esac
done

echo "Running stress test (--threads $THREADS --requests $REQUESTS)..."
"$BUILD_DIR/stress_test" -t "$THREADS" -r "$REQUESTS"
