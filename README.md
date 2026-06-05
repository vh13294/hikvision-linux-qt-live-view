# Hikvision HCNetSDK V6.1.9.4 — Linux 64-bit

Network camera / DVR management SDK with Qt5 GUI demo and console demo.  
Build date: **2022-04-12** | Architecture: **linux-x86_64**

---

## Recommended OS

**Recommended: Debian 12 (Bookworm) with LXQt**

Install runtime dependencies:
```bash
sudo apt install \
    libqt5widgets5 libqt5opengl5 \
    libsdl2-2.0-0 \
    libopenal1
```

---

## Releases

Pre-built binaries are attached to each [GitHub Release](https://github.com/vh13294/hikvision-linux-qt-live-view/releases) as `QtClientDemo-linux64.tar.gz` (binary + SDK libs + run script).

---

## QtLiveView — Auto-start Live View

A focused Qt5 app that reads a JSON config, logs into all configured cameras, and immediately starts streaming in a fullscreen grid — no manual interaction required.

### Configuration

Edit `build/liveview/config/DeviceConfig.json` before running:

```json
{
  "monitorIndex": 0,
  "gridSize": 2,
  "renderRaw": false,
  "hideVcaOverlay": false,
  "optimizeRender": false,
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
| `monitorIndex` | Monitor index (0 = primary) |
| `gridSize` | Grid dimension: `1`=1×1, `2`=2×2, `3`=3×3, `4`=4×4 |
| `renderRaw` | `true` = render raw video data directly, bypassing SDK overlay rendering |
| `hideVcaOverlay` | `true` = hide VCA overlay; used as fallback when `renderRaw` is unavailable |
| `optimizeRender` | `true` = enable PlayCtrl quality improvements: high-quality scaling, deblock, denoise (requires `renderRaw: true`) |
| `streamType` | `0` = main stream, `1` = sub stream |
| `channels` | List of channel numbers to display (NVR IP channels start at 33) |

### Running

```bash
bash build/liveview/run.sh
```

The script auto-detects whether to use software rendering: on a VM or a machine with no `/dev/dri/card*` device it sets `LIBGL_ALWAYS_SOFTWARE=1` automatically. Override with `HK_FORCE_SW_RENDER=1` (force software) or `HK_FORCE_SW_RENDER=0` (force hardware).

### Auto-start on Boot (systemd)

Create a systemd service so QtLiveView starts automatically after the display is ready.

**1. Create the service file:**

```bash
sudo nano /etc/systemd/system/qtliveview.service
```

```ini
[Unit]
Description=Hikvision QtLiveView
After=graphical.target

[Service]
Type=simple
WorkingDirectory=/home/your_username/path/to/build/liveview
User=your_username
Environment=DISPLAY=:0
Environment=XAUTHORITY=/home/your_username/.Xauthority
ExecStartPre=/bin/sleep 180
ExecStart=/home/your_username/path/to/build/liveview/run.sh
Restart=on-failure

[Install]
WantedBy=graphical.target
```

Replace `/home/your_username/path/to/build/liveview` with the actual path to your `build/liveview` folder.

`ExecStartPre=/bin/sleep 180` gives the display manager time to start before the app launches on boot. Reduce or remove it if starting from an already-running user session.

**2. Enable and start:**

```bash
sudo systemctl daemon-reload
sudo systemctl enable qtliveview.service
sudo systemctl start qtliveview.service
```

**3. Check status / logs:**

```bash
sudo systemctl status qtliveview.service
journalctl -u qtliveview.service -f
```

---

## Running the Qt Demo

Copy the entire `build/qt/` folder to your target machine and install the runtime dependencies:

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
