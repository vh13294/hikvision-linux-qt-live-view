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
| `hideVcaOverlay` | bool | Remove smart tracking/event boxes from all streams (see below) |

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

## hideVcaOverlay

When `true`, the app calls `NET_DVR_SetVCADrawMode` on each channel after login to instruct the device to stop burning smart overlay boxes (motion tracking, vehicle detection, line crossing, etc.) into the encoded video stream.

**Important notes:**
- This is a persistent camera/NVR setting — it remains active until changed back, even after the app closes.
- Because the overlay is removed at the source, **all clients** (Hik-Connect, iVMS, web browser) will see raw video without boxes. Push notifications and event recordings are unaffected.
- When connecting to an NVR IP, the setting is applied per channel (camera) through the NVR — no need to connect to individual cameras.
- Set back to `false` and restart the app to restore overlays on the device.

## Controls

| Input | Action |
|---|---|
| `Escape` | Quit |
| Right-click × 2 | Quit |

## Stream reconnection

The app retries failed streams automatically. On disconnect, the SDK attempts reconnection internally; if it gives up, the app re-queues the stream and retries every 60 seconds.
