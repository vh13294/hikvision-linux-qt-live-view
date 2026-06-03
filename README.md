# Hikvision HCNetSDK V6.1.9.4 — Linux 64-bit

Network camera / DVR management SDK with Qt5 GUI demo, console demo, and Java Swing demo.  
Build date: **2022-04-12** | Architecture: **linux-x86_64**

---

## Recommended OS for Running the Qt Binary

The precompiled SDK libraries (`lib/*.so`) were built for linux64 in April 2022.
The two hard constraints when choosing an OS are:

| Constraint | Requirement | Reason |
|---|---|---|
| **glibc** | >= 2.17 | Minimum ABI the SDK `.so` files were linked against |
| **OpenSSL ABI** | 1.1.x (`libssl.so.1.1`) | SDK bundles its own copy; system only needs to load it |
| **Qt runtime** | Qt 5.x | Compiled with Qt5; Qt6 ABI is not backward-compatible |

### Debian (recommended)

| Version | Codename | glibc | OpenSSL | Qt5 | Verdict |
|---|---|---|---|---|---|
| **Debian 11** | Bullseye | 2.31 | **1.1.x** (native) | 5.15 | **Best match** — OS was current when SDK shipped; OpenSSL 1.1.x matches natively; long-term support until 2026 |
| Debian 12 | Bookworm | 2.36 | 3.x (system) | 5.15 | Works — SDK bundles its own `libssl.so.1.1`, set `LD_LIBRARY_PATH` to use it |
| Debian 10 | Buster | 2.28 | 1.1.x | 5.11 | Works but EOL (April 2024); Qt 5.11 is old |
| Debian 9 | Stretch | 2.24 | 1.0.2 | 5.7 | EOL — avoid |

**Recommended: Debian 11 (Bullseye)**

Install runtime dependencies:
```bash
sudo apt install \
    libqt5widgets5 libqt5opengl5 \
    libsdl2-2.0-0 \
    libopenal1
```

### CentOS / RHEL Family

| Version | glibc | OpenSSL | Qt5 | Verdict |
|---|---|---|---|---|
| **Rocky Linux 8 / AlmaLinux 8** | 2.28 | **1.1.x** (native) | 5.15 | **Best match** — direct CentOS 8 replacement; supported until May 2029 |
| **Rocky Linux 9 / AlmaLinux 9** | 2.34 | 3.x (system) | 5.15 | Works — same bundled SSL workaround as Debian 12 |
| CentOS Stream 8 | 2.28 | 1.1.x | 5.15 | Works but rolling release, less stable for production |
| CentOS 7 | 2.17 | 1.0.x | 5.9 | EOL June 2024; Qt 5.9 is very old — avoid |

**Recommended: Rocky Linux 8 or AlmaLinux 8**

Install runtime dependencies:
```bash
sudo dnf install \
    qt5-qtbase qt5-qtbase-gui \
    SDL2 \
    openal-soft
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

For the best Qt rendering experience, use a Qt-native desktop environment: **LXQt** (lightweight) or **KDE Plasma** (full-featured). GNOME/XFCE work but apply extra theming overhead.

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
