#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly BUILD_DIR="${SCRIPT_DIR}/build"
readonly BINARY_NAME="filehopz"
readonly INSTALL_DIR="/usr/local/bin"
readonly INSTALL_PATH="${INSTALL_DIR}/${BINARY_NAME}"

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}"

sudo_cmd=()
if [[ ! -w "${INSTALL_DIR}" ]]; then
    sudo_cmd=(sudo)
fi

"${sudo_cmd[@]}" install -D -m 755 "${BUILD_DIR}/${BINARY_NAME}" "${INSTALL_PATH}"

echo "Installed ${BINARY_NAME} to ${INSTALL_PATH}"
