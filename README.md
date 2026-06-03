# Hikvision HCNetSDK V6.1.9.4 — Linux 64-bit

Network camera / DVR management SDK with Qt5 GUI demo, console demo, and Java Swing demo.  
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

## Building the Demos

Run inside the code-server terminal (or any Linux shell with the dependencies installed):

```bash
# Build everything
bash build-demos.sh all

# Qt GUI demo only
bash build-demos.sh qt

# Console demo only
bash build-demos.sh console
```

| Demo | Output binary | GUI |
|---|---|---|
| QtDemo | `build/qt/QtClientDemo` | Qt5 |
| consoleDemo | `build/console/lib/sdkTest` | None (CLI) |
| LinuxJavaDemo | built via `ant build` | Java Swing |

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
