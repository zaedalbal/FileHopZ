#!/bin/bash
set -euo pipefail

readonly BINARY_NAME="filehopz"
readonly INSTALL_DIR="/usr/local/bin"
readonly INSTALL_PATH="${INSTALL_DIR}/${BINARY_NAME}"

sudo_cmd=()
if [[ -e "${INSTALL_PATH}" && ! -w "${INSTALL_PATH}" ]] || [[ ! -e "${INSTALL_PATH}" && ! -w "${INSTALL_DIR}" ]]; then
    sudo_cmd=(sudo)
fi

"${sudo_cmd[@]}" rm -f -- "${INSTALL_PATH}"

echo "Removed ${INSTALL_PATH}"
