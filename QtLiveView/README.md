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

## Controls

| Input | Action |
|---|---|
| `Escape` | Quit |
| Right-click × 2 | Quit |

## Stream reconnection

The app retries failed streams automatically. On disconnect, the SDK attempts reconnection internally; if it gives up, the app re-queues the stream and retries every 60 seconds.
