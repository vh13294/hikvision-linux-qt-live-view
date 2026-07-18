# Qt Live View

Fullscreen multi-channel live viewer for Hikvision cameras and NVRs using the HCNetSDK.

## Requirements

- Qt 5.x or 6.x
- Hikvision HCNetSDK (Linux)
- HCNetSDKCom libraries placed in `lib/` next to the binary

## Building

```bash
qmake QtLiveView.pro
make -j$(nproc)
```

## Configuration

Edit `config/DeviceConfig.json` next to the binary before running.

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

### Top-level fields

| Field | Type | Description |
|---|---|---|
| `monitorIndex` | int | Monitor index to display on (0 = primary) |
| `gridSize` | int | Grid size — `2` = 2×2, `3` = 3×3, `4` = 4×4 |
| `hideVcaOverlay` | bool | Disable motion display on the device via `NET_DVR_SetDVRConfig` (persistent, affects all clients) |
| `hwDecode` | bool | Decode with FFmpeg + VAAPI (Intel GPU) instead of the SDK's software decoder. **Default: `true`** — set `false` to fall back to the SDK render pipeline |

### Device fields

| Field | Type | Description |
|---|---|---|
| `ip` | string | Camera or NVR IP address |
| `port` | int | SDK port, default `8000` |
| `username` | string | Login username |
| `password` | string | Login password |
| `streamType` | int | `0` = main stream, `1` = sub stream |
| `channels` | array | Channel numbers to display (NVR: camera index, camera: `1`) |

### Multiple devices

Add more objects to the `devices` array. Channels fill the grid left-to-right, top-to-bottom in the order listed.

```json
"devices": [
  { "ip": "192.168.1.10", "channels": [1, 2] },
  { "ip": "192.168.1.11", "channels": [1, 2] }
]
```

## hwDecode

Hikvision's Linux `libPlayCtrl.so` decodes in **software only** (the Windows build uses DXVA hardware decode). On low-power CPUs (e.g. Intel N5105) a full-HD stream can decode slower than real time, so the SDK buffers frames and the live-view delay grows without bound.

This mode is the **default** (`true`, also used when the key is absent from the config). When enabled, the app keeps HCNetSDK as transport (login, streaming, and reconnect over the single SDK port — works through an NVR with only port 8000 forwarded) but takes Hikvision's decoder out of the loop:

1. `NET_DVR_RealPlay_V40` runs with a raw data callback instead of `hPlayWnd`.
2. The MPEG-PS stream is demuxed and decoded by FFmpeg using **VAAPI** hardware acceleration on the Intel GPU (`/dev/dri/renderD128`). If VAAPI is unavailable, it falls back to FFmpeg software decode automatically.
3. Decoded frames are rendered by an OpenGL widget (YUV→RGB done in a shader on the GPU).

Latency is bounded by design: only the newest decoded frame is displayed, and if decode ever falls behind, the input backlog is dropped and the decoder resumes at the next keyframe — a brief glitch instead of ever-growing delay.

**Requirements on the target machine:**

```bash
sudo apt install libavcodec59 libavformat59 libavutil57   # or simply: sudo apt install ffmpeg
sudo apt install intel-media-va-driver vainfo             # Intel Gen11+ iGPU (N5105 = Jasper Lake)
vainfo   # should list the iHD driver with H264/HEVC decode profiles
```

The user running the app must have access to `/dev/dri/renderD128` (member of the `render` or `video` group).

Since the SDK render pipeline is bypassed, no SDK overlays (motion grid, tracking boxes, event markers) are shown in this mode. The device configuration is not touched, and other clients (phone app, iVMS) are unaffected.

> **Note:** `hwDecode` replaces the former `renderRaw` and `optimizeRender` flags, which have been removed. Those modes routed raw stream data into Hikvision's PlayCtrl decoder — software-only on Linux, so full-HD decode could fall behind real time and live-view delay accumulated without bound.

## hideVcaOverlay

Hides overlays at the source when using the default SDK render mode. When `true`, the app reads the full channel picture config from the device via `NET_DVR_GetDVRConfig` (`NET_DVR_PICCFG_V40`, falling back to `NET_DVR_PICCFG_V30`), sets `struMotion.byEnableDisplay = 0`, and writes it back with `NET_DVR_SetDVRConfig`.

**Important notes:**
- This is a **persistent device setting** — it remains in effect until changed back, even after the app closes.
- Because the overlay is disabled at the source, **all clients** (Hik-Connect, iVMS, web browser) will see raw video without motion detection boxes. Push notifications and event recordings are unaffected.
- When connecting to an NVR IP, the setting is applied per channel through the NVR — no need to connect to individual cameras.
- Set back to `false` and restart the app to restore the display on the device.
- Motion detection itself (alerts, recordings) continues to run normally — only the visual display is suppressed.

## Controls

| Input | Action |
|---|---|
| `Escape` | Quit |
| Right-click × 2 | Quit |

## Stream reconnection

The app retries failed streams automatically. On disconnect, the SDK attempts reconnection internally; if it gives up, the app re-queues the stream and retries every 60 seconds.
