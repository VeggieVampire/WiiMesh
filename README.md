# WiiMesh

WiiMesh is a read-only Nintendo Wii homebrew Meshtastic message client written in C++ for devkitPPC and libogc.

This first version intentionally starts with USB enumeration and diagnostics before assuming a serial chipset. If the connected device exposes USB CDC ACM, WiiMesh will attempt a read-only Meshtastic serial client connection. Other chipsets are reported in the diagnostic screen and `debug.log` so their correct driver can be added from real hardware data.

## Tested Hardware

Current real-Wii USB testing has only been done with:

- RAKwireless WisBlock Meshtastic Starter Kit US915
- RAK19007 base board
- RAK4631 nRF52840 core
- USB VID `0x239a`, PID `0x8029`
- CDC-style interfaces:
  - Interface 0: CDC control, class `0x02`, subclass `0x02`, endpoint `0x81`
  - Interface 1: CDC data, class `0x0a`, bulk OUT `0x01`, bulk IN `0x82`

For that hardware, WiiMesh has successfully sent `ToRadio.want_config_id` using USB bulk write to endpoint `0x01`, received Meshtastic `FromRadio` frames, parsed node information, decoded direct `TEXT_MESSAGE_APP` packets, displayed received direct messages on the Wii, and saved them to `messages.dat`.

As of `v0.1.51`, the known-good real-hardware path is direct-message receive from a second Meshtastic device through the RAK4631 over USB CDC. Channel-message receive and other USB serial chipsets still need separate hardware confirmation.

Other Meshtastic USB devices and serial chipsets are not confirmed yet. They should be treated as diagnostic targets until their Wii USB descriptors and setup requirements are captured.

## USB Device Overrides

WiiMesh creates this file on first launch:

```text
SD:/apps/wii-mesh/USB.config
```

Standard USB CDC ACM devices are tried automatically from their USB descriptors. For future Meshtastic boards that show bulk IN/OUT endpoints but are not standard CDC, edit `USB.config` instead of rebuilding `boot.dol`:

```text
# Try one exact device as CDC:
force_cdc=303a:1001

# Try any product under one vendor, useful for ESP32-S3 tests:
force_cdc=303a:*

# Last-resort diagnostic mode. Use briefly only:
try_any_bulk_cdc=1

# Generic USB control transfer:
# control=VID:PID,bmRequestType,bRequest,wValue,wIndex,hexData

# CP210x 115200 8N1 + DTR/RTS example:
force_cdc=10c4:ea60
control=10c4:ea60,0x41,0x00,0x0001,0x0000,
control=10c4:ea60,0x41,0x07,0x0303,0x0000,
control=10c4:ea60,0x41,0x03,0x0800,0x0000,
control=10c4:ea60,0x41,0x1e,0x0000,0x0000,00c20100
```

`try_any_bulk_cdc=1` can grab non-serial USB devices, so turn it back off after collecting diagnostics. If WiiMesh reports `cp210x-uart-needs-driver`, the board is not native CDC and needs a CP210x driver rather than a config-only override.

The same setup helpers are available on the Wii under **Settings -> USB**. That screen can scan USB devices, try opening the current candidate, add common ESP32-S3 override rules, add the CP210x 115200 recipe, apply configured USB control transfers, briefly enable bulk diagnostic mode, reset `USB.config`, reassert CDC line controls, and resend the Meshtastic config wake command.

With Wii IP/UDP enabled, the same operations can be done without FTPii:

```bat
py outputs\send_udp_command.py 192.168.0.42 "USB_CONFIG_CP210X"
py outputs\send_udp_command.py 192.168.0.42 "USB_OPEN"
py outputs\send_udp_command.py 192.168.0.42 "USB_CONTROLS"
py outputs\send_udp_command.py 192.168.0.42 "CFG"
```

## Features

- Produces `boot.dol` with devkitPPC/libogc.
- Enumerates Wii USB devices and shows VID, PID, class, subclass, protocol, interfaces, and endpoints.
- Implements a transport boundary with a libogc USB transport and a mock transport.
- Implements Meshtastic streaming frames and a minimal protobuf decoder for:
  - `ToRadio.want_config_id` startup request
  - `FromRadio.my_info`
  - `FromRadio.node_info`
  - `FromRadio.packet.decoded`
  - `TEXT_MESSAGE_APP`
  - channel-vs-direct detection
  - sender node ID and known sender names
- Provides a TV-safe receive UI navigable with the Wii Remote D-pad.
- Draws a graphics-backed dashboard skin under the UI text using libogc/gxflux.
- Loads an optional custom GUI background from `SD:/apps/wii-mesh/theme/background.rgb565`.
- Saves the last 100 messages to `SD:/apps/wii-mesh/messages.dat`.
- Saves diagnostics to `SD:/apps/wii-mesh/debug.log`.
- Handles unplug/reconnect by returning to enumeration and reopening supported serial devices.

## Editable Icons

WiiMesh uses `outputs/icons` as the main editable icon folder during local builds and FTPii deploys.

- Dashboard/UI icons live at the top level: `home.png`, `node.png`, `channels.png`, `chat.png`, `map.png`, `settings.png`, and `bell.png`.
- Meshtastic emoji icons live in `outputs/icons/emojis` and are named by Unicode codepoint, for example `emoji_u1f514_bell.png`.
- Emoji PNGs are generated from the Windows emoji/symbol font when missing, then reused on future builds so edited files stay editable.
- The generator caches Unicode's official `emoji-test.txt` list and compiles a broad single-codepoint emoji atlas before adding live mesh-log codepoints.
- The emoji generator also scans fetched Wii logs for live `U+...` codepoints from Meshtastic usernames/messages and adds missing editable `emoji_u...` files automatically.
- The older `outputs/ui_icons` folder is still supported as a fallback, but new edits should go in `outputs/icons`.

## Build

Install devkitPro with the Wii package group, then run `make`.

### Windows

1. Install devkitPro from the official installer.
2. Open **devkitPro MSYS2** from the Start menu.
3. Run:

```sh
pacman -Syu
pacman -S wii-dev git make
git clone https://github.com/VeggieVampire/WiiMesh.git
cd WiiMesh
make clean
make
```

### Linux or macOS

Install devkitPro pacman using the official devkitPro instructions, then run:

```sh
sudo dkp-pacman -Syu
sudo dkp-pacman -S wii-dev
git clone https://github.com/VeggieVampire/WiiMesh.git
cd WiiMesh
make clean
make
```

### Existing Checkout

If you already cloned the repo:

```sh
cd WiiMesh
make clean
make
```

The output is:

```text
boot.dol
```

Copy it to the Homebrew Channel app folder:

```text
SD:/apps/wii-mesh/boot.dol
```

For a local SD card mounted on Windows as drive `E:`, the copy command is:

```bat
mkdir E:\apps\wii-mesh
copy boot.dol E:\apps\wii-mesh\boot.dol
copy meta.xml E:\apps\wii-mesh\meta.xml
copy assets\icon.png E:\apps\wii-mesh\icon.png
```

If FTPii is running on the Wii, this repo also includes a Windows deploy helper:

```bat
tools\deploy_ftpii.bat 192.168.0.13
```

Replace `192.168.0.13` with the IP shown by FTPii.

## Custom GUI Theme Background

WiiMesh can load a custom full-screen GUI skin from:

```text
SD:/apps/wii-mesh/theme/background.rgb565
```

Convert a PNG/JPG/BMP image on Windows:

```bat
tools\make_theme_background.bat my-theme.png background.rgb565
```

If the converter says Pillow is missing:

```bat
py -m pip install pillow
```

Copy the generated file to:

```text
SD:/apps/wii-mesh/theme/background.rgb565
```

See [docs/themes.md](docs/themes.md) for theme image notes.

## GUI Layout Editor

WiiMesh includes a Windows-friendly Python editor for planning dashboard section
positions and labels:

```bat
tools\mesh_layout_editor.bat
```

It saves a flat config named `MeshLayout.config`, intended for:

```text
SD:/apps/wii-mesh/theme/MeshLayout.config
```

See [docs/layout-editor.md](docs/layout-editor.md).

After enabling IP/UDP in WiiMesh with the `+` button, the editor can upload,
download, reload, and import live sample data over UDP without FTPii.

You can also upload from the command line:

```bat
tools\upload_layout_udp.bat 192.168.0.13 MeshLayout.config
```

This sends `LAYOUT_SET` commands for every config value and then sends
`LAYOUT_SAVE`.

## Direct MIDI Clip Transfers

WiiMesh can receive small `.mid`/`.midi` files as direct Meshtastic messages.
Transfers must be sent as direct messages to the WiiMesh node, not to LongFast
or another channel. Completed MIDI files are saved under:

```text
SD:/apps/wii-mesh/received_files/
```

On real Wii hardware, a completed MIDI transfer is parsed and auto-played once.
It will also appear in the MIDI player in Settings -> MIDI / FILES.

The wire format is plain text:

```text
[START] filename.mid b64
[CHUNK] 1/3 filename.mid b64:BASE64_TEXT_PART_1
[CHUNK] 2/3 filename.mid b64:BASE64_TEXT_PART_2
[CHUNK] 3/3 filename.mid b64:BASE64_TEXT_PART_3
[END] filename.mid
```

Rules:

- Send every line as a direct message to the WiiMesh device/user.
- Keep chunks small enough for Meshtastic text messages; the helper uses 140
  base64 characters per chunk.
- Use a simple filename with no slashes, such as `doorbell.mid`.
- Send chunks in order. WiiMesh replies with `[MFACK]` direct messages as it
  receives START, CHUNK, and END.
- The MIDI auto-play path is intentionally receive-side only; public channel
  file-transfer commands are ignored.

To generate a tiny test MIDI and the exact direct-message lines:

```bat
tools\make_meshfile_midi_test.bat
```

The helper writes:

```text
outputs/wiimesh_test.mid
outputs/meshfile_midi_direct_messages.txt
```

## Host Mock Test

The parser and mock transport can be exercised without Wii hardware:

```sh
make host-test
```

This requires a host C++ compiler such as `g++`. It compiles and runs a native executable that feeds a synthetic Meshtastic text frame into the protocol layer.

## Real Wii USB Data Collection

See [docs/collect-usb-info.md](docs/collect-usb-info.md). In short:

1. Copy `boot.dol` to `SD:/apps/wii-mesh/boot.dol`.
2. Start WiiMesh from the Homebrew Channel with no Meshtastic device connected.
3. Plug the Meshtastic device into a Wii USB port.
4. Open the USB diagnostic screen and record VID, PID, interfaces, endpoints, and class/subclass/protocol.
5. Retrieve `SD:/apps/wii-mesh/debug.log` and use that data to add the exact serial driver.

## Current USB Driver Status

Implemented:

- USB enumeration and descriptor logging.
- CDC ACM bulk endpoint selection for the tested RAK4631/TinyUSB-style device.
- Basic CDC ACM line coding/control setup.
- Working RAK4631 serial startup write path: synchronous bulk write to endpoint `0x01`.
- Read-only bulk RX for Meshtastic serial frames.
- Meshtastic node/config stream display on the Wii.

Not yet implemented:

- CP210x vendor-specific serial setup.
- CH34x vendor-specific serial setup.
- FTDI vendor-specific serial setup.
- Message sending.

Those are deliberately gated on real Wii diagnostic logs so the project does not guess the attached chipset.
