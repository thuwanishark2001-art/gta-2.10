#!/bin/bash
# Kingmakers RP: Google Cloud VM startup script (Ubuntu 24.04, e2-micro).
# Runs at every boot; the full setup only happens on the first one.
set -euo pipefail
exec > >(tee -a /var/log/kingmakers-setup.log) 2>&1
DIR=/opt/kingmakers

if [ -f "$DIR/.setup-done" ]; then
    echo "KINGMAKERS: already set up, nothing to do"; exit 0
fi

# e2-micro has 1 GB RAM: add 2 GB swap so the image build doesn't run out.
if ! swapon --show | grep -q /swapfile; then
    fallocate -l 2G /swapfile && chmod 600 /swapfile && mkswap /swapfile && swapon /swapfile
    grep -q /swapfile /etc/fstab || echo '/swapfile none swap sw 0 0' >> /etc/fstab
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update -y
apt-get install -y ca-certificates curl git unzip openssl
if ! command -v docker >/dev/null; then
    curl -fsSL https://get.docker.com | sh
fi
systemctl enable --now docker

if [ ! -d "$DIR/.git" ]; then
    git clone -b claude/project-thread-hcfqpn https://github.com/thuwanishark2001-art/gta-2.10.git "$DIR"
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
touch "$DIR/.setup-done"
docker compose ps
ss -ulnp | grep 7777 || true
echo "KINGMAKERS READY: public IP $(curl -s -H 'Metadata-Flavor: Google' http://metadata.google.internal/computeMetadata/v1/instance/network-interfaces/0/access-configs/0/external-ip)"
