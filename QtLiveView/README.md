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
| `renderRaw` | bool | Bypass HCPreview pipeline: raw callback → PlayCtrl decode (no overlays, phone unaffected) |
| `hideVcaOverlay` | bool | Fallback: disable motion display on the device via `NET_DVR_SetDVRConfig` (persistent, affects all clients) |

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

## renderRaw

When `true`, each stream bypasses the Hikvision HCPreview rendering pipeline entirely. Instead of passing `hPlayWnd` to the SDK, the app receives the raw encoded stream via a data callback and feeds it directly into PlayCtrl (`libPlayCtrl.so`) for decode and display.

**Result:** no smart overlays (motion grid, tracking boxes, event markers) are visible on screen. The phone app, Hik-Connect, and any other client are completely unaffected because this is a client-side change only.

**How it works:**
1. `NET_DVR_RealPlay_V40` is called with `hPlayWnd = NULL` and `rawDataCallback` as the data callback.
2. On the first `NET_DVR_SYSHEAD` packet, the callback calls `PlayM4_OpenStream` and `PlayM4_Play` to start the PlayCtrl decoder on the frame's window.
3. Subsequent `NET_DVR_STREAMDATA` packets are fed via `PlayM4_InputData` — PlayCtrl decodes and renders them directly with no overlay pipeline.

**Trade-offs vs default mode:**
- No overlays of any kind — both smart detection boxes and device-side OSD are stripped.
- PlayCtrl renders without the hardware-accelerated display path that HCPreview uses; performance is comparable but may differ on low-end hardware.
- The device configuration is not touched — no persistent changes are made.

**Fallback behaviour:** if PlayCtrl setup fails for a stream (e.g. `PlayM4_GetPort` or `PlayM4_OpenStream` returns an error), that stream falls back to normal HCPreview rendering. If `hideVcaOverlay` is also `true`, the device-side config is applied for that stream before falling back, suppressing the overlay at the source.

## hideVcaOverlay

Fallback mode for when `renderRaw` is unavailable or fails. When `true`, the app reads the full channel picture config from the device via `NET_DVR_GetDVRConfig` (`NET_DVR_PICCFG_V40`, falling back to `NET_DVR_PICCFG_V30`), sets `struMotion.byEnableDisplay = 0`, and writes it back with `NET_DVR_SetDVRConfig`.

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
