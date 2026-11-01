#!/bin/bash
# Kingmakers RP: Oracle Cloud "Always Free" VM setup (cloud-init user data).
#
# Paste this whole file into "Show advanced options -> Management ->
# Paste cloud-init script" when creating an Ubuntu 24.04 instance on the
# VM.Standard.E2.1.Micro (AMD, 1 GB RAM) shape. It runs once, on first boot,
# as root, and is safe to re-run by hand: sudo bash oracle-cloud-init.sh
#
# Progress: /var/log/kingmakers-setup.log on the VM
# (sudo tail -f /var/log/kingmakers-setup.log from Cloud Shell / SSH).
set -euo pipefail
exec > >(tee -a /var/log/kingmakers-setup.log) 2>&1
DIR=/opt/kingmakers
BRANCH=claude/project-thread-hcfqpn
PORT=7777

echo "KINGMAKERS: setup starting $(date -u)"

# The Micro shape has 1 GB RAM: 2 GB swap keeps the image build and MariaDB alive.
if ! swapon --show | grep -q /swapfile; then
    fallocate -l 2G /swapfile && chmod 600 /swapfile && mkswap /swapfile && swapon /swapfile
    grep -q /swapfile /etc/fstab || echo '/swapfile none swap sw 0 0' >> /etc/fstab
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update -y
apt-get install -y ca-certificates curl git unzip openssl iptables-persistent

# Oracle's Ubuntu images ship iptables rules that reject everything except SSH,
# on top of the cloud Security List. Allow the game port in the VM as well.
if ! iptables -C INPUT -p udp --dport "$PORT" -j ACCEPT 2>/dev/null; then
    iptables -I INPUT 1 -p udp --dport "$PORT" -j ACCEPT
fi
netfilter-persistent save || true

if ! command -v docker >/dev/null; then
    curl -fsSL https://get.docker.com | sh
fi
systemctl enable --now docker

if [ ! -d "$DIR/.git" ]; then
    git clone -b "$BRANCH" https://github.com/thuwanishark2001-art/gta-2.10.git "$DIR"
fi
cd "$DIR/server"
if [ ! -f .env ]; then
    cp .env.example .env
    sed -i "s/^DB_PASSWORD=.*/DB_PASSWORD=$(openssl rand -hex 16)/" .env
    sed -i "s/^DB_ROOT_PASSWORD=.*/DB_ROOT_PASSWORD=$(openssl rand -hex 16)/" .env
    sed -i "s/^KM_RCON_PASSWORD=.*/KM_RCON_PASSWORD=/" .env
    chmod 600 .env
fi

./scripts/deploy.sh
docker compose ps
ss -ulnp | grep ":$PORT" || true
echo "KINGMAKERS READY: public IP $(curl -s https://api.ipify.org || echo unknown) port $PORT/udp"
