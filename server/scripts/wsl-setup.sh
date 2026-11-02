#!/bin/bash
# Kingmakers RP: host the server on a Windows PC (WSL2 Ubuntu + playit.gg).
#
# Run inside the Ubuntu app (WSL2), not in PowerShell:
#   curl -fsSLo wsl-setup.sh https://raw.githubusercontent.com/thuwanishark2001-art/gta-2.10/claude/proceed-igki1c/server/scripts/wsl-setup.sh
#   bash wsl-setup.sh
#
# Installs Docker and the playit.gg agent inside Ubuntu, deploys the server
# (docker compose, UDP 7777 inside WSL), and installs a `kingmakers` command
# that starts everything and keeps the playit tunnel open. playit runs inside
# WSL too, so no Windows port forwarding or router setup is needed.
set -euo pipefail
DIR="$HOME/kingmakers"
BRANCH=claude/project-thread-hcfqpn

if ! grep -qi microsoft /proc/version 2>/dev/null; then
    echo "Note: this doesn't look like WSL; continuing anyway."
fi

echo "== packages"
sudo apt-get update -y
sudo apt-get install -y ca-certificates curl git unzip openssl gpg

echo "== docker"
if ! command -v docker >/dev/null; then
    curl -fsSL https://get.docker.com -o /tmp/get-docker.sh
    sudo sh /tmp/get-docker.sh
fi
sudo usermod -aG docker "$USER" || true
if [ "$(ps -p 1 -o comm=)" = "systemd" ]; then
    sudo systemctl enable --now docker
else
    sudo service docker start
fi
for i in $(seq 1 30); do sudo docker info >/dev/null 2>&1 && break; sleep 1; done

echo "== playit.gg agent"
if ! command -v playit >/dev/null; then
    curl -fsSL https://playit-cloud.github.io/ppa/key.gpg | gpg --dearmor | sudo tee /etc/apt/trusted.gpg.d/playit.gpg >/dev/null
    echo "deb [signed-by=/etc/apt/trusted.gpg.d/playit.gpg] https://playit-cloud.github.io/ppa/data ./" | sudo tee /etc/apt/sources.list.d/playit-cloud.list >/dev/null
    sudo apt-get update -y
    sudo apt-get install -y playit
fi

echo "== server code"
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

echo "== deploy (first run downloads and builds; takes a while)"
# This shell is not in the docker group until the next login, so use sudo.
sudo -E env "PATH=$PATH" ./scripts/deploy.sh

echo "== installing the 'kingmakers' command"
sudo tee /usr/local/bin/kingmakers >/dev/null <<EOF
#!/bin/bash
# Starts the Kingmakers server and the playit.gg tunnel. Keep this window open.
set -e
if ! sudo docker info >/dev/null 2>&1; then
    if [ "\$(ps -p 1 -o comm=)" = "systemd" ]; then sudo systemctl start docker; else sudo service docker start; fi
    for i in \$(seq 1 30); do sudo docker info >/dev/null 2>&1 && break; sleep 1; done
fi
cd "$DIR/server"
sudo docker compose up -d
sudo docker compose ps
echo
echo "Server running. Starting the playit.gg tunnel; keep this window open."
echo "Close it (or press Ctrl+C) to take the server offline for players."
exec playit
EOF
sudo chmod +x /usr/local/bin/kingmakers

echo
echo "KINGMAKERS READY on this PC (UDP 7777 inside WSL)."
echo "Next: the playit.gg agent starts now. Open the link it prints, sign in,"
echo "and add a tunnel: type UDP, local address 127.0.0.1, local port 7777."
echo "From now on, start everything with:  kingmakers"
echo
exec playit
