# Collecting USB Diagnostic Information on a Real Wii

WiiMesh must identify the actual USB serial chipset before adding a chipset-specific driver. Meshtastic devices can appear as USB CDC ACM, CP210x, CH34x, FTDI, or another adapter depending on the board.

## Setup

1. Build WiiMesh with `make`.
2. Copy `boot.dol` to `SD:/apps/wii-mesh/boot.dol`.
3. Create the directory `SD:/apps/wii-mesh/` if it does not exist.
4. Launch WiiMesh from the Homebrew Channel.

## Capture Steps

1. Start with the Meshtastic device unplugged.
2. Wait for the UI to show that no supported serial device is connected.
3. Plug the Meshtastic device into one Wii USB port.
4. Press `1` on the Wii Remote to switch to the USB diagnostic screen.
5. Record:
   - Vendor ID
   - Product ID
   - Device class, subclass, protocol
   - Every interface number
   - Interface class, subclass, protocol
   - Endpoint addresses and attributes
6. Press `1` again to return to messages.
7. Power off the Wii or exit the application.
8. Copy `SD:/apps/wii-mesh/debug.log` from the SD card.

## What to Send Back for Driver Work

Paste the diagnostic screen contents and attach `debug.log`. The driver decision should be based on the descriptor data:

- CDC ACM usually has a communications interface (`0x02/0x02/0x01`) and a data interface (`0x0a`) with bulk IN/OUT endpoints.
- CP210x, CH34x, and FTDI usually expose vendor-specific interfaces (`0xff`) and require vendor control transfers before serial reads work.

WiiMesh currently attempts CDC ACM only after detecting CDC-style descriptors. Vendor-specific devices are logged but left unopened until the correct chipset is known.
