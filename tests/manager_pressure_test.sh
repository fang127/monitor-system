#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/Debug}"
BINARY="${MANAGER_STRESS_TEST_BIN:-${BUILD_DIR}/tests/manager_stress_test}"
MANAGER_ADDR="${MANAGER_ADDR:-localhost:50051}"

FIRST_ARG="${1:-}"
if [[ -n "${FIRST_ARG}" && "${FIRST_ARG:0:2}" != "--" ]]; then
  MANAGER_ADDR="$1"
  shift
fi

if [[ ! -x "${BINARY}" ]]; then
  cat >&2 <<EOF
manager_stress_test binary not found: ${BINARY}

Build it first, for example:
  cmake --build ${BUILD_DIR} --target manager_stress_test

You can override paths with:
  BUILD_DIR=/path/to/build ${0} ${MANAGER_ADDR}
  MANAGER_STRESS_TEST_BIN=/path/to/manager_stress_test ${0} ${MANAGER_ADDR}
EOF
  exit 127
fi

exec "${BINARY}" \
  --manager "${MANAGER_ADDR}" \
  --mode auto \
  "$@"
