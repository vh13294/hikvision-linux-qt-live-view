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

After a fresh Debian 12 LXQt install you can run the Ansible playbook (if available) to automate all of the steps below — systemd service setup, caffeine install, and cron configuration. Otherwise, follow the manual steps.

**1. Install caffeine** — prevents screen sleep and screensaver from interrupting the live view:

```bash
sudo apt update && sudo apt install caffeine -y
```

**2. Create the service file:**

```bash
sudo nano /etc/systemd/system/qtliveview.service
```

```ini
[Unit]
Description=QtLiveView auto-start
After=network.target

[Service]
Type=oneshot
User=your_username
Environment=DISPLAY=:0
Environment=XAUTHORITY=/home/your_username/.Xauthority
ExecStartPre=/bin/sleep 20
ExecStart=/usr/bin/caffeinate /home/your_username/Downloads/QtLiveView/run.sh

[Install]
WantedBy=multi-user.target
```

Replace `your_username` with your actual Linux username and update the path to `run.sh` if needed.

- `ExecStartPre=/bin/sleep 20` gives the display manager a moment to settle before the app launches.
- `caffeinate` keeps the session awake (no screen sleep or screensaver) for the lifetime of the process.

**3. Enable and start:**

```bash
sudo systemctl daemon-reload
sudo systemctl enable qtliveview.service
sudo systemctl start qtliveview.service
```

**4. Check status / logs:**

```bash
sudo systemctl status qtliveview.service
journalctl -u qtliveview.service -f
```

---

### Scheduled Shutdown (cron)

To shut down the machine automatically every night at 22:00, add a cron job.

**Open the root crontab:**

```bash
sudo crontab -e
```

On first run it will ask you to choose an editor — pick `nano` if unsure.

**Add this line at the bottom:**

```
0 22 * * * /usr/sbin/shutdown -h now
```

Save and exit (`Ctrl+O`, `Enter`, `Ctrl+X` in nano).

**Cron field reference:**

```
┌─ minute  (0–59)
│ ┌─ hour   (0–23)
│ │ ┌─ day of month (1–31)
│ │ │ ┌─ month (1–12)
│ │ │ │ ┌─ day of week (0–7, 0 and 7 = Sunday)
│ │ │ │ │
0 22 * * *   /usr/sbin/shutdown -h now
```

**Verify the cron job was saved:**

```bash
sudo crontab -l
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
