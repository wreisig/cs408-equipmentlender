#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 -h PUBLIC_IP -i PATH_TO_PEM"
}

host=
identity=
while getopts ":h:i:" option; do
  case "$option" in
    h) host="$OPTARG" ;;
    i) identity="$OPTARG" ;;
    *) usage >&2; exit 2 ;;
  esac
done
if [[ -z "$host" || -z "$identity" ]]; then
  usage >&2
  exit 2
fi
if [[ ! -r "$identity" ]]; then
  echo "SSH key not readable: $identity" >&2
  exit 1
fi
if ! command -v ssh >/dev/null 2>&1 || ! command -v scp >/dev/null 2>&1; then
  echo "This script requires ssh and scp on your laptop." >&2
  exit 1
fi

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ssh_options=(-i "$identity" -o StrictHostKeyChecking=accept-new)

if command -v rsync >/dev/null 2>&1; then
  rsync -az --delete \
    --exclude '.git/' --exclude '*.pem' --exclude '.env' \
    -e "ssh ${ssh_options[*]}" \
    "$root/" "ubuntu@${host}:/opt/equipment-library/"
else
  if ! command -v tar >/dev/null 2>&1; then
    echo "This script requires rsync, or tar as a fallback, on your laptop." >&2
    exit 1
  fi
  tar --exclude='.git' --exclude='*.pem' --exclude='.env' -czf - -C "$root" . |
    ssh "${ssh_options[@]}" "ubuntu@${host}" \
      'mkdir -p /opt/equipment-library && tar -xzf - -C /opt/equipment-library'
fi

ssh "${ssh_options[@]}" "ubuntu@${host}" \
  'cd /opt/equipment-library && mkdir -p data && gcc -std=c11 -Wall -Wextra -Werror -O2 app/main.c -o app/equipment-library && sudo systemctl restart equipment-library'

curl --fail --silent --show-error --max-time 10 "http://${host}/api/health"
printf '\nDeployed successfully: http://%s/\n' "$host"
