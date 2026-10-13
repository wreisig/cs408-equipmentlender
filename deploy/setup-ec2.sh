#!/usr/bin/env bash
set -euo pipefail

if [[ "$(id -u)" -ne 0 ]]; then
  echo "Run this script on the EC2 server with sudo: sudo bash deploy/setup-ec2.sh" >&2
  exit 1
fi

apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y gcc sqlite3 nginx rsync

install -d -o ubuntu -g ubuntu /opt/equipment-library
install -d -o ubuntu -g ubuntu /opt/equipment-library/data
install -m 0644 deploy/equipment-library.service \
  /etc/systemd/system/equipment-library.service
install -m 0644 deploy/nginx-equipment-library.conf \
  /etc/nginx/sites-available/equipment-library
ln -sfn /etc/nginx/sites-available/equipment-library \
  /etc/nginx/sites-enabled/equipment-library
rm -f /etc/nginx/sites-enabled/default

nginx -t
systemctl daemon-reload
systemctl enable nginx equipment-library
systemctl restart nginx
echo "EC2 setup complete. Deploy the application from your laptop with deploy/deploy.sh."
