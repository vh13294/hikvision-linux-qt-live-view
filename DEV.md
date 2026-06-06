# Development Guide

---

## Development Environment (Docker + code-server)

For building and editing the source on any host OS (Windows, macOS, Linux),
use the included Docker setup. It provides a full Debian 12 build environment
with Qt5 tools and VS Code in the browser.

**Start:**
```bash
docker compose build
docker compose up -d
```

The container uses the LinuxServer.io PUID/PGID pattern: the entrypoint runs as root,
calls `usermod`/`groupmod` to remap the `developer` user to the configured IDs, then
drops privileges before starting code-server. Edit `PUID`/`PGID` in `docker-compose.yml`
to match your host user (`id -u` / `id -g`). The default is `0`/`0` (root) for Proxmox LXC.

**Open:** `http://localhost:9015` — no password required (port is bound to `0.0.0.0`, reachable from any device on the same network).

---

## Building the Demos

Run inside the code-server terminal (or any Linux shell with the dependencies installed):

```bash
# Build everything
bash build-demos.sh all

# Qt GUI demo only
bash build-demos.sh qt

# Live view only
bash build-demos.sh liveview

# Console demo only
bash build-demos.sh console
```

| Demo | Output binary | GUI |
|---|---|---|
| QtLiveView | `build/liveview/QtLiveView` | Qt5 fullscreen grid |
| QtDemo | `build/qt/QtClientDemo` | Qt5 |
| consoleDemo | `build/console/lib/sdkTest` | None (CLI) |

---

## Publishing a Release

Tag and push to trigger the GitHub Actions release workflow:

```bash
git tag v1.0.0
git push origin v1.0.0
```

Pre-built binaries are attached to each [GitHub Release](https://github.com/vh13294/hikvision-linux-qt-live-view/releases) as `QtClientDemo-linux64.tar.gz`.
