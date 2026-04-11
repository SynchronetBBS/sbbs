#!/usr/bin/env bash
set -euo pipefail

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_ROOT="${1:-${SELF_DIR}/../../../webv4}"
SRC_ROOT="${SELF_DIR}/files"

if [[ ! -d "${TARGET_ROOT}" ]]; then
  echo "ERROR: target does not exist: ${TARGET_ROOT}" >&2
  exit 1
fi

if [[ ! -d "${SRC_ROOT}" ]]; then
  echo "ERROR: source pack is missing: ${SRC_ROOT}" >&2
  exit 1
fi

backup_root="${TARGET_ROOT}/.zzt-web-backup/$(date +%Y%m%d_%H%M%S)"
mkdir -p "${backup_root}/root/js" "${backup_root}/root/api"

backup_if_exists() {
  local rel="$1"
  local src="${TARGET_ROOT}/${rel}"
  local dst="${backup_root}/${rel}"
  if [[ -f "${src}" ]]; then
    mkdir -p "$(dirname "${dst}")"
    cp -a "${src}" "${dst}"
  fi
}

install_file() {
  local rel="$1"
  local src="${SRC_ROOT}/${rel}"
  local dst="${TARGET_ROOT}/${rel}"
  if [[ ! -f "${src}" ]]; then
    echo "ERROR: missing packaged file: ${src}" >&2
    exit 1
  fi
  mkdir -p "$(dirname "${dst}")"
  cp -a "${src}" "${dst}"
  echo "Installed: ${dst}"
}

backup_if_exists "root/terminal.xjs"
backup_if_exists "root/terminal-iframe.html"
backup_if_exists "root/js/terminal.js"
backup_if_exists "root/js/flweb.js"
backup_if_exists "root/api/flweb-assets.ssjs"
backup_if_exists "root/api/terminal-ui-config.ssjs"
backup_if_exists "root/terminal-ui.ini"

install_file "root/terminal.xjs"
install_file "root/terminal-iframe.html"
install_file "root/js/terminal.js"
install_file "root/js/flweb.js"
install_file "root/api/flweb-assets.ssjs"
install_file "root/api/terminal-ui-config.ssjs"
install_file "root/terminal-ui.ini"

echo
echo "ZZT web bridge install complete."
echo "Backup saved to: ${backup_root}"
echo "Target root: ${TARGET_ROOT}"
echo
echo "Multimedia terminal page available at:"
echo "  http://<your-bbs-host>:<port>/terminal.xjs"
