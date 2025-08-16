# Matter-Smart-Lamp-Simulation (ESP32)

A tiny ESP32 firmware + Python tools that **imitate key Matter behaviors** (mDNS advertising, TLV commands over UDP, basic clusters) without depending on the full ESP-Matter stack. It drives an LED via PWM and lets you control **On/Off**, **Level (brightness)**, **Identify blink**, and a **Delayed-Off timer**—all using a super-small custom TLV codec.

> For a concise background and design rationale, see `Smart Lamp Matter Project Report Final.pdf`.

## Highlights

* `_matter._udp` **mDNS** advertisement with TXT records (vendor/product, discriminator, setup PIN).
* Minimal “clusters” on endpoint 0:

  * **On/Off (0x0006)** — on, off, toggle + *Delayed-Off* attribute (`0x4001`, seconds).
  * **Level Control (0x0008)** — set/read brightness (0–254 → PWM duty).
  * **Identify (0x0003)** — 6 Hz blink for N seconds.
  * **Descriptor (0x001D)** & **Basic (0x0028)** — tiny attribute responses.
* Lightweight **TLV encoder/decoder** (`src/tlv.h`) for UInt8/UInt16 and byte blobs.
* FreeRTOS helpers for **identify blinking** and **delayed-off countdown**.
* Python controller scripts (no Matter stack needed): scan, CLI, attribute reads, “dev-mode” PASE demo.

---

## Repository layout

```
src/
  main.cpp           # ESP32 firmware: Wi-Fi + mDNS + UDP + TLV handlers + PWM
  tlv.h              # Minimal TLV definitions + encode/decode helpers
  scripts/
    bridge.py        # Interactive CLI: on/off/toggle/level/timer/identify/info/desc
    tlv.py           # TLV builders/decoders used by the CLI
    scan_mdns.py     # Zeroconf browser for _matter._udp
    pase_flow.py     # Dev-mode PASE flow demo (PBKDF → PAKE1 → PAKE3)
    Random_pass.py   # Random salt/verifier generator (helper)
Smart Lamp Matter Project Report Final.pdf
```

---

## Hardware

* ESP32 DevKit (tested with typical ESP32-WROOM)
* LED on **GPIO 2** (default; changeable)
* PWM: channel 0, **500 Hz**, **8-bit** resolution

---

## Build & Flash

> Works with **Arduino IDE** or **PlatformIO**.

1. **Configure Wi-Fi & pins**
   Open `src/main.cpp` and change:

```cpp
const char* ssid     = "<YOUR_WIFI_SSID>";
const char* password = "<YOUR_WIFI_PASSWORD>";
const int   ledPin   = 2;     // change if not using GPIO 2
```

2. **Build & Upload**

* **Arduino IDE**: Select your ESP32 board → set correct port → Upload.
* **PlatformIO** (example):

  ```ini
  ; platformio.ini (example)
  [env:esp32dev]
  platform = espressif32
  board = esp32dev
  framework = arduino
  upload_speed = 921600
  monitor_speed = 115200
  ```

  Then: `pio run -t upload -t monitor`

3. **Expected serial log**

```
Connecting to WiFi...
Connected! IP=192.168.x.y
IPv6 (link-local)=fe80::...
UDP listening on 5540
mDNS _matter published.
```

---

## Discovery

The device announces `_matter._udp` via mDNS with TXT records:

* `VP=0xFFF1+0x8000` (Vendor+Product)
* `DT=0x0101` (Device Type)
* `CM=2` (Commissioning Mode)
* `DN=SmartLamp` (Device Name)
* `SII=4520` (Discriminator)
* `CRI=300` (Commissioning Retry)
* `PI=20202021` (Setup PIN)

Scan from your PC:

```bash
python -m pip install zeroconf
python src/scripts/scan_mdns.py
```

---

## Python CLI (controller)

Install deps:

```bash
python -m pip install qrcode zeroconf
```

Run the controller:

```bash
python src/scripts/bridge.py
```

You’ll see a QR payload (for demo) and a prompt. Supported commands:

```
on | off | toggle
level <0-100> | level?
timer <sec>   | timer?
identify <sec>
info          # Basic cluster (0x0028)
desc          # Descriptor cluster (0x001D)
exit
```

---

## What the firmware actually implements

### TLV basics (custom & tiny)

* Unsigned Int (base `0x04`).
* Tags are **0–7**, stored in the top 3 bits of the first byte.
* `UInt8`: `[ 0x04 | (tag<<5) | 0x00, 0x01, <value> ]`
* `UInt16`: `[ 0x04 | (tag<<5) | 0x01, <lo>, <hi> ]`
* Byte strings: `[ 0x10 | (tag<<5), <len>, <bytes...> ]`

Each command packet includes:

* **tag 0** → endpoint (always `0`)
* **tag 1** → cluster (UInt16, LE)
* **tag 2** → command (UInt8)

### Clusters / Commands

| Cluster | Name          |  Cmd | Meaning                                 |
| ------: | ------------- | ---: | --------------------------------------- |
|  0x0006 | On/Off        | 0x01 | ON (restores previous level if 0)       |
|         |               | 0x00 | OFF                                     |
|         |               | 0x02 | TOGGLE or **write** `DelayedOff`        |
|         |               | 0x03 | **read** `DelayedOff`                   |
|  0x0008 | Level Control | 0x04 | Set level (`tag 3 = 0–254`)             |
|         |               | 0x03 | Read attribute `0x0000` (current level) |
|  0x0003 | Identify      | 0x00 | Blink for N seconds (`tag 0 = seconds`) |
|  0x001D | Descriptor    | 0x01 | Minimal endpoint & server cluster list  |
|  0x0028 | Basic         | 0x01 | Minimal vendor/product response         |

**Attributes**

* On/Off → `DelayedOff` id **0x4001** (seconds)
* Level  → Current Level id **0x0000** (0–254)

**Replies**

* Most “action” commands: respond with ASCII `ACK`.
* Attribute reads: respond with TLV; the value is returned in **tag 5** alongside the requested **tag 4** (attribute id).

### Example TLVs (hex)

* **ON** (cluster `0x0006`, cmd `0x01`):
  `04 01 00  25 06 00  44 01 01`
  (ep=0, cluster=0x0006, cmd=01)

* **Set Level = 50% (≈127/254)**:
  `04 01 00  25 08 00  44 01 04  63 01 7F`
  (ep=0, cluster=0x0008, cmd=04, tag3=0x7F)

* **Read Level (attr 0x0000)**:
  `04 01 00  25 08 00  44 01 03  85 00 00`
  → reply includes `tag4=0x0000` and `tag5=<level>`

* **Write DelayedOff = 30s**:
  `04 01 00  25 06 00  44 01 02  85 01 40  85 1E 00`

---

## Runtime behaviors

* **Identify**: blinks at \~6 Hz using a FreeRTOS task; restores previous LED state afterwards.
* **Delayed-Off**: spawns a FreeRTOS task that waits N seconds and turns the lamp off (clears countdown state).
* **Level/PWM**: if you send `ON` with level 0, firmware picks a default \~50% (127).

---

## Security & Limitations

* The included **PASE** demo (`pase_flow.py`) is **development-mode only**: not cryptographically complete, not CASE-secured, and **not** interoperable with Apple/Google ecosystems. It exists solely to observe state transitions.
* UDP and TLV here are intentionally simple and **not secure**. Use on a trusted LAN only.
* Don’t commit real Wi-Fi credentials or setup codes.

---

## Troubleshooting

* **Nothing appears in `scan_mdns.py`** → Ensure host and ESP32 are on the same subnet; allow multicast/UDP in your OS/firewall.
* **CLI only prints `ACK`** → You issued a command that doesn’t return TLV (e.g., `on`, `toggle`). Use `level?` or `timer?` for a TLV response.
* **Brightness looks inverted** → Check LED wiring; `ledPin` expects active-high PWM on GPIO 2 by default.
* **“mDNS error!”** → Try rebooting the ESP32 or your router; mDNS responders sometimes need a reset on Wi-Fi reconnection.

---

## Dev-mode commissioning demo

You can observe a toy PBKDF + PASE flow:

```bash
python src/scripts/pase_flow.py
```

It sends:

1. **PBKDFParamRequest** → firmware responds with a mock salt/params
2. **PAKE1** → firmware replies **PAKE2**
3. **PAKE3** → firmware ACKs and marks “paired (dev)”

Again: this is **not** secure and not meant for real commissioning.

---

## Roadmap / Ideas

* Real attribute table & reporting/subscribe.
* Non-volatile state (restore on reboot).
* Transition times/fades for Level Control.
* Migration to ESP-Matter for full spec compliance (CASE, attestation, clusters).

---

## License

MIT © Sina Beyrami

---

## Acknowledgments

* Matter specification & test vectors (CSA).
* ESP32 Arduino core and FreeRTOS under the hood.

---
