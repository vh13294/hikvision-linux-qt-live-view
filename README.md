# Hikvision HCNetSDK V6.1.9.4 — Linux 64-bit

Network camera / DVR management SDK with Qt5 GUI demo and console demo.  
Build date: **2022-04-12** | Architecture: **linux-x86_64**

---

## Recommended OS for Running the Qt Binary

**Recommended: Debian 12 (Bookworm) with LXQt**

Install runtime dependencies:
```bash
sudo apt install \
    libqt5widgets5 libqt5opengl5 \
    libsdl2-2.0-0 \
    libopenal1
```

---

## Development Environment (Docker + code-server)

For building and editing the source on any host OS (Windows, macOS, Linux),
use the included Docker setup. It provides a full Debian 12 build environment
with Qt5 tools and VS Code in the browser.

**Start:**
```bash
DOCKER_BUILDKIT=0 docker compose build
DOCKER_BUILDKIT=0 docker compose up -d
```

The container uses the LinuxServer.io PUID/PGID pattern: the entrypoint runs as root,
calls `usermod`/`groupmod` to remap the `developer` user to the configured IDs, then
drops privileges before starting code-server. Edit `PUID`/`PGID` in `docker-compose.yml`
to match your host user (`id -u` / `id -g`). The default is `0`/`0` (root) for Proxmox LXC.

**Open:** `http://localhost:9015` — no password required (port is bound to `0.0.0.0`, reachable from any device on the same network).

> **Why `DOCKER_BUILDKIT=0`:** BuildKit runs as a separate daemon that reads `/proc/stat` to report
> resource usage after exporting layers. On Proxmox LXC it can be OOM-killed mid-export (this image is
> large — Qt5 + build tools + code-server), severing the gRPC connection between the Docker CLI and the
> BuildKit daemon, which surfaces as `transport endpoint is not connected`. Disabling BuildKit falls back
> to the classic in-process builder which has no separate daemon to lose connection to.

---

## Releases

Pre-built binaries are attached to each [GitHub Release](https://github.com/vh13294/hikvision-linux-qt-live-view/releases) as `QtClientDemo-linux64.tar.gz` (binary + SDK libs + run script).

To publish a new release, tag and push:

```bash
git tag v1.0.0
git push origin v1.0.0
```

GitHub Actions will build the Qt demo on Ubuntu 22.04 and attach the packaged binary to the release automatically.

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

## QtLiveView — Auto-start Live View

A focused Qt5 app that reads a JSON config, logs into all configured cameras, and immediately starts streaming in a fullscreen grid — no manual interaction required. Mirrors the [hikvision-dotnet-sdk](https://github.com/vh13294/hikvision-dotnet-sdk) preview demo in C++/Qt.

### Configuration

Edit `build/liveview/config/DeviceConfig.json` before running:

```json
{
  "displayScreen": 0,
  "numberOfScreen": 2,
  "devices": [
    {
      "ip": "192.168.1.100",
      "port": 8000,
      "username": "admin",
      "password": "12345",
      "streamType": 0,
      "channels": [1, 2, 3, 4]
    }
  ]
}
```

| Field | Description |
|---|---|
| `displayScreen` | Monitor index (0 = primary) |
| `numberOfScreen` | Grid dimension: `1`=1×1, `2`=2×2, `3`=3×3, `4`=4×4 |
| `streamType` | `0` = main stream, `1` = sub stream |
| `channels` | List of channel numbers to display (NVR IP channels start at 33) |

### Running

```bash
bash build/liveview/run.sh
```

---

## Running the Qt Demo

Build the binary (see above). The build script automatically copies `lib/` and `translation/` into `build/qt/`, producing a self-contained directory. Zip and copy the entire `build/qt/` folder to your target machine and install the runtime dependencies:

```bash
sudo apt install libqt5widgets5 libqt5opengl5 libsdl2-2.0-0 libopenal1
```

Run from inside `build/qt/`:

```bash
./run.sh
```

The generated `run.sh` sets `LD_LIBRARY_PATH` relative to its own location, so the folder can be placed anywhere.

---

## Bundled Open Source Components

| Library | Version | License |
|---|---|---|
| OpenSSL | 1.0.2 | OpenSSL / SSLeay |
| libiconv | 1.9.2 | LGPL v2 |
| TinyXML | 2.6.0 | zlib |
| libsrtp2 | 2.2.0 | BSD |
| cJSON | — | MIT |

Full license texts: [doc/Open Source Software Licenses-HCNetSDK.txt](doc/Open%20Source%20Software%20Licenses-HCNetSDK.txt)
