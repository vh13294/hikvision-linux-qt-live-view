# Hikvision HCNetSDK V6.1.9.4 — Linux 64-bit

Network camera / DVR management SDK with Qt5 GUI demo and console demo.  
Build date: **2022-04-12** | Architecture: **linux-x86_64**

---

## Recommended OS

**Recommended: Debian 12 (Bookworm) with LXQt**

---

## Releases

Pre-built binaries are attached to each [GitHub Release](https://github.com/vh13294/hikvision-linux-qt-live-view/releases) as `QtClientDemo-linux64.tar.gz` (binary + SDK libs + run script).

After downloading, make the binary and run script executable (the execute bit is lost if the files pass through a zip, a Windows machine, or a FAT/exFAT USB stick — the app then fails with `Permission denied`):

```bash
chmod +x QtLiveView run.sh
```

---

## QtLiveView — Auto-start Live View

A focused Qt5 app that reads a JSON config, logs into all configured cameras, and immediately starts streaming in a fullscreen grid — no manual interaction required.

### Configuration

Edit `build/liveview/config/DeviceConfig.json` before running:

```json
{
  "monitorIndex": 0,
  "gridSize": 2,
  "hideVcaOverlay": false,
  "hwDecode": true,
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
| `hideVcaOverlay` | `true` = hide VCA overlay on the device itself (persistent, affects all clients) |
| `hwDecode` | Default `true`: decode with FFmpeg + VAAPI (Intel GPU) instead of the SDK's software decoder — fixes accumulating live-view delay on Linux; no SDK overlays are shown. Set `false` to use the SDK's own render pipeline (overlays visible, software decode) |
| `streamType` | `0` = main stream, `1` = sub stream |
| `channels` | List of channel numbers to display (NVR IP channels start at 33) |

### Running

Install runtime dependencies on the target machine:

```bash
# Qt runtime
sudo apt install libqt5widgets5 libqt5opengl5 libsdl2-2.0-0 libopenal1

# hwDecode: Intel VAAPI driver (N5105/Jasper Lake and newer use iHD).
# FFmpeg itself is bundled in lib/ inside the release tarball — no need to
# apt install ffmpeg separately, and no risk of the release binary needing a
# libavformat SONAME your distro's FFmpeg package doesn't ship.
sudo apt install intel-media-va-driver vainfo

# verify: should list the iHD driver with H264/HEVC decode profiles
vainfo
```

For hardware decode (`hwDecode: true`) the user running the app must be in the `video`/`render` groups to access `/dev/dri/renderD128`:

```bash
sudo usermod -aG video,render $USER   # then log out and back in
```

If VAAPI is missing the app still runs — it falls back to FFmpeg software decode with bounded (non-accumulating) latency.

```bash
bash build/liveview/run.sh
```

The script auto-detects whether to use software rendering: on a VM or a machine with no `/dev/dri/card*` device it sets `LIBGL_ALWAYS_SOFTWARE=1` automatically. Override with `HK_FORCE_SW_RENDER=1` (force software) or `HK_FORCE_SW_RENDER=0` (force hardware).

### Auto-start on Boot (systemd)

After a fresh Debian 12 LXQt install you can run the Ansible playbook (if available) to automate all of the steps below — systemd service setup, caffeine install, and cron configuration. Otherwise, follow the manual steps.

The service below attaches to `DISPLAY=:0` of an already-running graphical session — it does not open one itself. Without auto-login, the box stops at the login screen after boot/reboot and the service fails (no display to attach to), so skip the login screen first.

**1. Skip the login screen (auto-login)** — for SDDM, add a drop-in config rather than editing `/etc/sddm.conf` directly (packages can overwrite it):

```bash
sudo mkdir -p /etc/sddm.conf.d
sudo nano /etc/sddm.conf.d/autologin.conf
```

```ini
[Autologin]
User=your_username
Session=lxqt.desktop
```

Check the exact session filename first with `ls /usr/share/xsessions/` — use whatever matches there for `Session=`.

Reboot to confirm it boots straight to the desktop with no login prompt.

**2. Install caffeine** — prevents screen sleep and screensaver from interrupting the live view:

```bash
sudo apt update && sudo apt install caffeine -y
```

**3. Create the service file:**

```bash
sudo nano /etc/systemd/system/qtliveview.service
```

```ini
[Unit]
Description=QtLiveView auto-start
After=graphical.target

[Service]
Type=simple
User=your_username
Environment=DISPLAY=:0
Environment=XAUTHORITY=/home/your_username/.Xauthority
Environment=XDG_RUNTIME_DIR=/run/user/1000
ExecStartPre=/bin/sleep 20
ExecStart=/usr/bin/caffeinate /bin/bash /home/your_username/Downloads/QtLiveView/run.sh
Restart=on-failure
RestartSec=5

[Install]
WantedBy=graphical.target
```

Replace `your_username` with your actual Linux username, `1000` in `XDG_RUNTIME_DIR` with your user's id (`id -u your_username`), and update the path to `run.sh` if needed.

- `After=graphical.target` / `WantedBy=graphical.target` — the hardware decode path (`hwDecode: true`) creates an OpenGL context, so the service must start after the display session is up, not just the network.
- `XDG_RUNTIME_DIR` is required by Qt when launched outside a login session.
- `ExecStartPre=/bin/sleep 20` gives the display manager a moment to settle before the app launches.
- `Restart=on-failure` relaunches the viewer if it exits with an error (e.g. the GL context wasn't ready yet).
- `caffeinate` keeps the session awake (no screen sleep or screensaver) for the lifetime of the process.
- `run.sh` is invoked through `/bin/bash` so the service works even if the script lost its execute bit (common after copying or unzipping — otherwise the service fails with `Permission denied`; fixable alternatively with `chmod +x run.sh`).
- For hardware decode, `your_username` must also be in the `video`/`render` groups — see [Running the Qt Demo](#running-the-qt-demo).

**4. Enable and start:**

```bash
sudo systemctl daemon-reload
sudo systemctl enable qtliveview.service
sudo systemctl start qtliveview.service
```

**5. Check status / logs:**

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

This demo renders through the SDK's own PlayCtrl/SuperRender pipeline, not FFmpeg — there's no `hwDecode` option or VAAPI dependency here (that's specific to [QtLiveView](#qtliveview--auto-start-live-view)).

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

The `QtLiveView-linux64.tar.gz` release additionally bundles Ubuntu 22.04's FFmpeg runtime (`libavformat`/`libavcodec`/`libavutil`) and its own codec/container dependencies in `lib/`, so the release doesn't depend on the target distro's FFmpeg SONAME. This pulls in GPL-licensed codec libraries (notably `libx264`, and possibly `libx265`/`libxvidcore` depending on the runner's package set) — check the exact set with `ldd lib/libavcodec.so.*` on a given release. Corresponding source for these is the unmodified upstream project source, available at:
- FFmpeg (LGPL 2.1+/GPL 2+ depending on build config): https://ffmpeg.org/
- x264 (GPL 2+): https://code.videolan.org/videolan/x264
- x265 (GPL 2+): https://bitbucket.org/multicoreware/x265_git

All are built unmodified from Ubuntu 22.04's official archive packages — see `apt-get source <package>` for the exact revision matching a given release.
