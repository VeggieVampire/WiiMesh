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
- Saves the last 100 messages to `SD:/apps/wii-mesh/messages.dat`.
- Saves diagnostics to `SD:/apps/wii-mesh/debug.log`.
- Handles unplug/reconnect by returning to enumeration and reopening supported serial devices.

## Build

Install devkitPro with devkitPPC, libogc, and libfat, then run:

```sh
make
```

The output is:

```text
boot.dol
```

Copy it to:

```text
SD:/apps/wii-mesh/boot.dol
```

## Host Mock Test

The parser and mock transport can be exercised without Wii hardware:

```sh
make host-test
```

This compiles a native executable that feeds a synthetic Meshtastic text frame into the protocol layer.

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
