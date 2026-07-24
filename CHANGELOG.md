# Changelog

All notable WiiMesh test-build changes are tracked here so real-Wii testing does not depend on chat history.

## 0.1.31 - 2026-07-24

- Moves the UI lower into the TV-safe area and removes the top pseudo-logo clutter.
- Draws a cleaner boxed header and tab area so the screen reads more like an app and less like a debug dump.
- Refreshes the local node display when MyNodeInfo arrives after NodeInfo.

## 0.1.30 - 2026-07-24

- Reworks the TV UI into Meshtastic-style tabs: Messages, Nodes, and Stream.
- Removes the unbounded frame/timer counter from the top-right corner.
- Keeps USB diagnostics behind button 1 and uses button 2 or D-pad left/right for tab switching.

## 0.1.29 - 2026-07-24

- Adds a TV-visible identity panel with the local node ID and known node IDs/names.
- Adds a streaming packet log on screen and over UDP with `STREAM` lines for Wii-to-RAK config requests and RAK-to-Wii FromRadio payloads.
- Tracks last packet source, destination, channel, and port to make direct-vs-channel traffic visible.

## 0.1.28 - 2026-07-24

- Adds a TV-visible protocol event pane showing MyNodeInfo, NodeInfo, channels, config completion, non-text packet ports, and received text messages.
- Adds on-screen node and text-message counters.
- Logs protocol events with a `PROTO` prefix over UDP for easier parser debugging.

## 0.1.27 - 2026-07-24

- Makes the 1000-ish USB write matrix opt-in instead of running during automatic startup config.
- Adds UDP command `MATRIX` to run the broad USB write matrix on demand.
- Keeps UDP command `CFG` on the simpler single sync write path so the app can boot and network logging can come up first.

## 0.1.26 - 2026-07-24

- Expands the USB startup write test into a broad matrix of 1000-ish structured attempts.
- Varies prep sequence, endpoint address, packet length, and transfer API.
- Stops immediately and logs `WORKING USB WRITE ...` if any variant succeeds; otherwise logs total attempts failed.

## 0.1.25 - 2026-07-24

- Tries multiple startup write variants in a single Wii run: bulk sync 6-byte, bulk sync 64-byte padded, interrupt sync 6-byte, and bulk async 6-byte.
- Logs each attempt as `write try ...` and logs the first successful variant as `WORKING USB WRITE ...`.
- Keeps the USB handle open until all write variants fail, so one test build produces a complete comparison.

## 0.1.24 - 2026-07-24

- Switches the first Meshtastic startup write from async bulk USB to synchronous bulk USB.
- Aligns USB transfer buffers to 32 bytes and flushes the transmit buffer before writing.
- Logs `sync write` results so the next Wii run can distinguish async-submit issues from endpoint/interface issues.

## 0.1.23 - 2026-07-24

- Treats libogc USB handles as opaque signed values instead of assuming negative means closed.
- Adds an explicit USB transport open flag so the RAK4631 handle stays usable after descriptor/setup calls succeed.

## 0.1.22 - 2026-07-24

- Avoids opening the same RAK/TinyUSB CDC device twice during setup.
- Sends CDC line coding/DTR using the kept-open data handle only, to test whether closing the second handle was invalidating the first.
- Adds the USB file descriptor value to CDC setup/detail logs.

## 0.1.21 - 2026-07-24

- Fixed a DSI crash mapped to `MessageStore::save()` by no longer rewriting `messages.dat` when no messages have changed.
- Stopped polling the Meshtastic serial reader until the startup config request has actually been sent.
- Made the UDP `CFG` command switch the app into read mode when it succeeds.

## 0.1.20 - 2026-07-24

- Keeps the CDC serial file descriptor open when an async read submit/completion fails.
- Prevents early read failures from closing USB before the Meshtastic config write can run.
- Shows read failure details without forcing a reconnect loop.

## 0.1.19 - 2026-07-24

- Updated the default UDP debug target to this Codex PC's Wi-Fi IP, `192.168.0.233`.
- Updated `outputs/debug-target.txt`; no code rebuild is required if that file is deployed.

## 0.1.18 - 2026-07-24

- Changed USB bulk OUT writes to use `USB_WriteBlkMsgAsync`.
- Added async write submit/completion diagnostics on the USB detail/log path.
- Keeps the Meshtastic startup request buffered until the async write callback completes.

## 0.1.17 - 2026-07-24

- Updated WiiMesh network logging to match the provided Muscogee Wii network test source pattern.
- Calls `net_init()` before `if_config()`.
- Opens UDP sockets with `IPPROTO_IP`.
- Binds the log socket to local UDP port `44014`.
- Uses the libogc/Wii sockaddr length of `8` for bind/send operations.

## 0.1.16 - 2026-07-24

- Changed UDP debug from broadcast to direct PC target, matching the provided MvskokeTV Wii network test approach.
- Added `sd:/apps/wii-mesh/debug-target.txt`; if present, its first line is used as the PC log target IP.
- Defaults the debug target to `192.168.0.110`, matching the provided network test package.

## 0.1.15 - 2026-07-24

- Added network socket return details to the `NET LOG` line.
- Calls `net_init()` before socket creation after DHCP setup to test libogc socket initialization ordering.
- Added explicit USB write early-failure detail for closed file descriptors and missing bulk OUT endpoints.
- Logs USB write failure before closing the serial file descriptor.

## 0.1.14 - 2026-07-24

- Removed legacy deploy mirroring to `sd:/apps/WiiMesh/`.
- FTPii deployment now targets only `sd:/apps/wii-mesh/`.

## 0.1.13 - 2026-07-24

- Fixed network debug initialization to use libogc DHCP setup via `if_config()`.
- Displays the Wii IP address on the `NET LOG` line when network logging is active.
- Displays DHCP/socket failure return details when network logging fails.

## 0.1.12 - 2026-07-24

- Added a UDP command listener on port `44016`.
- Added diagnostic commands:
  - `PING`: proves the command channel is reaching WiiMesh.
  - `CDC`: reasserts CDC line coding/DTR/RTS and clears endpoint halts.
  - `CFG`: resends the Meshtastic serial config request.
  - `SCAN`: reruns USB enumeration and logs descriptors.
- Added PC-side UDP command sender scripts.

## 0.1.11 - 2026-07-24

- Added periodic UDP heartbeat logs every few seconds while WiiMesh is running.
- Sends each UDP log line to both `255.255.255.255` and the Wii's local `/24` directed broadcast address.
- Bumped the visible version marker to `WiiMesh v0.1.11`.

## 0.1.10 - 2026-07-24

- Added UDP broadcast debug logging on port `44015`.
- Added a visible `NET LOG` status line.
- Added Windows listener scripts under `tools/` and `outputs/` for live network logs.
- Bumped the visible version marker to `WiiMesh v0.1.10`.

## 0.1.9 - 2026-07-24

- Added a visible `WiiMesh v0.1.9` marker to the top of the app screen so testers can confirm the current build is running.
- Changed the log line label to `LOG STATUS` so it is harder to miss in photos.
- The FTPii deploy script now mirrors uploads to both `sd:/apps/wii-mesh/` and legacy `sd:/apps/WiiMesh/` to avoid accidentally launching an older installed copy.

## 0.1.8 - 2026-07-24

- Changed `debug.log` writes to unbuffered binary append mode with sequence numbers instead of timestamps.
- Added an on-screen log status line showing whether `debug.log` opened successfully.
- Logs USB detail lines whenever config startup writes fail or retry.

## 0.1.7 - 2026-07-24

- Added a visible USB detail line showing CDC endpoint/setup/write return values.
- Calls `USB_SetConfiguration`, `USB_SetAlternativeInterface`, and `USB_ClearHalt` on the CDC data interface before serial use.
- Shows `Config TX failed; retrying` when the Meshtastic startup frame cannot be written.
- Retries the Meshtastic config request every two seconds while the USB serial interface remains open.

## 0.1.6 - 2026-07-24

- Added a Homebrew Channel `icon.png` using the Meshtastic green/dark palette and mark.
- Added the icon to the source tree under `assets/icon.png`; the FTPii deploy script already uploads `outputs/icon.png` when present.

## 0.1.5 - 2026-07-24

- Added on-screen RX byte, RX frame, bad-frame, and TX byte counters.
- Sends CDC line coding and DTR/RTS control state to the matching CDC control interface when libogc exposes TinyUSB control and data interfaces separately.
- Keeps a fallback CDC setup attempt on the data interface file descriptor and logs both control-request results.
- Verified current Meshtastic protobuf field numbers against the official `mesh.proto`; no protobuf field-number changes were needed.

## 0.1.4 - 2026-07-24

- Fixed RAK4631/TinyUSB CDC detection for `VID 0x239a PID 0x8029`.
- Allows known nRF52840 CDC devices to open from the data interface entry when libogc lists the CDC control and data interfaces as separate USB entries.
- Uses the detected bulk endpoints from interface `0x0a` (`0x01` OUT and `0x82` IN in the photographed RAK4631 test) for Meshtastic serial framing.

## 0.1.3 - 2026-07-24

- Added RAK/nRF52840 USB candidate recognition for common VID families (`0x239a`, `0x1915`) while still requiring endpoint descriptors before opening serial.
- Changed USB enumeration to keep raw device-list entries on screen even when `USB_OpenDevice` or `USB_GetDescriptors` fails.
- Added per-device diagnostic text to the USB diagnostic screen, including descriptor/open failures.
- Added USB raw device-entry scan counts to `debug.log`.
- Changed the no-driver status text to direct testers to the diagnostic screen instead of only saying unsupported.

## 0.1.2 - 2026-07-24

- Fixed console/framebuffer initialization order to avoid a blank TV screen at startup.
- Added an on-screen frame counter heartbeat so testers can tell whether the UI loop is alive.
- Draws an initial `Booting WiiMesh` screen before SD/log initialization.
- Changed runtime app data paths to `sd:/apps/wii-mesh/` to match the FTPii deploy folder.
- Broadened USB enumeration attempts across CDC, data, vendor-specific, HID, and device-class-zero USB listings.
- Changed USB bulk reads to asynchronous polling so opening a quiet serial device should not freeze the UI.
- Added `outputs/fetch_ftpii_logs.bat` for manual log retrieval.

## 0.1.1 - 2026-07-24

- Added curl-based FTPii deployment script targeting `/sd/apps/wii-mesh/`.
- Added Homebrew Channel `meta.xml`.
- Built and exported a first `boot.dol` test artifact.

## 0.1.0 - 2026-07-24

- Created the initial C++ devkitPPC/libogc Wii homebrew project.
- Added USB descriptor enumeration and diagnostic logging.
- Added CDC ACM detection and read-only serial framing scaffold.
- Added minimal Meshtastic serial frame and protobuf parsing for node info and text messages.
- Added TV-safe message UI, D-pad scrolling, SD message persistence, and debug logging.
- Added mock transport and parser test harness for non-Wii development.
