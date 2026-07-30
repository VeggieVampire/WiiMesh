# Changelog

All notable WiiMesh test-build changes are tracked here so real-Wii testing does not depend on chat history.

## Tools - 2026-07-29

- FTPii deploy now prefetches the remote theme folder listing and `theme/MeshLayout.config` before uploading.
- FTPii deploy preserves Wii-side `GUI.config` and `theme/MeshLayout.config` by default.
- `MeshLayout.config` upload is now opt-in with `--upload-layout`; theme background upload is opt-in with `--upload-theme`.
- FTPii log fetch now also pulls remote listings, `messages.dat`, `GUI.config`, and `theme/MeshLayout.config`.

## 0.1.89 - 2026-07-30

- Draws the unread bell as transparent line art instead of a filled yellow square.
- Moves right-side menu labels into the graphics layer so they render below icons instead of on top of them.
- Keeps the saved `GUI.config` untouched during the bell/menu drawing fixes.

## 0.1.88 - 2026-07-30

- Keeps the unread bell visible after new messages until the user actively opens a message/chat.
- Stops clearing the unread bell just because the Chats tab or Chat Detail screen is visible.

## 0.1.87 - 2026-07-29

- Mounts the SD card before UI initialization so saved `GUI.config` is loaded on boot.
- Makes placement calibration create `SD:/apps/wii-mesh` before saving `GUI.config`.
- Verifies `GUI.config` after saving and only advances zones when the save succeeds.

## 0.1.86 - 2026-07-29

- Centers each right-side dashboard menu label independently under its own icon.
- Moves menu icons slightly upward inside their tiles to create a dedicated label row.
- Keeps `GUI.config` untouched during the menu label adjustment.

## 0.1.85 - 2026-07-29

- Changes placement calibration so `B` only exits the editor.
- Changes resize to hold `1` plus D-pad, removing the old `B` resize conflict.
- Makes `A` save `GUI.config` and advance to the next editable zone.
- Adds a brighter selected-zone highlight with corner handles and a clearer zone list.

## 0.1.84 - 2026-07-29

- Stops `-` from exiting placement calibration once the editor is open.
- Uses `-/+` inside calibration to shrink/grow the selected zone's text width.
- Changes calibration save to `1`, with `B` as the only normal editor exit.

## 0.1.83 - 2026-07-29

- Centers the right-side menu text labels under their icon tiles.
- Shortens right-side menu labels to `HOME`, `NODE`, `CHAN`, `CHAT`, `MAP`, and `SET`.
- Removes the extra `MENU` text header from the icon rail to reduce overlap on TV output.

## 0.1.82 - 2026-07-29

- Moves each right-side menu label underneath its icon instead of beside it.
- Gives the vertical menu slots a little more height so icon and label have separate space.
- Narrows the default menu text width for the under-icon labels.

## 0.1.81 - 2026-07-29

- Changes primary menu navigation to Up/Down for the vertical menu rail.
- Uses Left/Right for list item selection in Chats, Node Options, and Map.
- Adds an unread-message bell indicator instead of only showing the bell for alert-bell text.
- Adds a movable/resizable `bell` zone to `GUI.config`.
- Marks messages as seen when viewing Chats or Chat Detail so the unread bell clears.
- Fixes calibration so `B` exits, while holding `B` plus D-pad resizes the selected zone.

## 0.1.80 - 2026-07-29

- Moves the primary icon menu from the top strip to a right-side vertical rail.
- Shrinks the main content/header/footer cards so text and panels do not run under the menu rail.
- Extends `GUI.config` zones with `w`, `h`, and `text_width`.
- Calibration mode now supports D-pad movement, hold `B` plus D-pad resizing, `1/2` text-width adjustment, and `+` saving.
- Makes text follow zone offsets so moved graphics and labels stay together.
- Clips content/header/menu/footer text to their configured widths to reduce overrun outside boxes.

## 0.1.79 - 2026-07-29

- Adds on-Wii GUI zone nudging while the calibration checkerboard is open.
- Uses `A` to cycle editable zones: header, nav, content, and footer.
- Uses the D-pad to move the selected zone in 4px steps.
- Uses `2` to save the zone offsets to `SD:/apps/wii-mesh/GUI.config`.
- Loads `GUI.config` on startup and applies saved offsets to the graphics zones.
- Updates the FTPii deploy script to pull `GUI.config` before uploading a new build.

## 0.1.78 - 2026-07-29

- Adds a temporary placement calibration screen toggled with the Wii Remote Minus button.
- Calibration shows a large 64px checkerboard, full-screen border, safe-area border, console text border, and labeled UI zones.
- Adds row/column labels so real-TV photos can be used to correct WiiMesh graphics/text placement.

## 0.1.77 - 2026-07-29

- Cleans up the graphics layout after the first graphic skin test.
- Aligns graphic panels to the console text grid so labels/content sit inside their cards.
- Replaces competing split-panel decoration with one large content card, header card, nav strip, and footer card.
- Simplifies the navigation text row to show only the selected icon/window.
- Moves the bell status marker away from header text.

## 0.1.76 - 2026-07-29

- Adds a built-in graphics skin behind the reliable basic UI.
- Draws dark Meshtastic-style panels, highlighted icon tiles, signal bars, map lines, chat bubbles, and footer chrome.
- Adds a visible bell marker in the header when the latest message is an alert-bell message.
- Keeps the text/navigation behavior from v0.1.75 so graphics do not break the working controls.

## 0.1.75 - 2026-07-29

- Adds a six-icon Meshtastic-style primary navigation row: Home, Node Options, Channels, Chats, Map, and Settings.
- Changes highlighting a primary icon to immediately show that window.
- Makes Chat Detail, Log, Debug, and Status sub-windows instead of primary tabs.
- Makes `B` return from Chat Detail to Chats and from Log/Debug/Status to Settings.
- Keeps alert bell messages visible as `[BELL]` and ACK packets visible as `[OK]`.

## 0.1.74 - 2026-07-29

- Adjusts the basic UI toward Meshtastic UI's screen flow: Home first, then Chats, Nodes, Chat, Log, Debug, and Status.
- Adds a Home Dashboard with radio health, primary channel, device identity, counts, storage note, and latest message.
- Renames the message list to Chats / Messages so it behaves more like MUI's chat list.
- Keeps Log and Debug as WiiMesh's practical Tools area for packet troubleshooting.

## 0.1.73 - 2026-07-29

- Replaces the complex dashboard/layout UI with a basic readable Wii-safe tab interface.
- Keeps core tabs for Messages, Nodes, Chat, Stream, Debug, and Status.
- Makes selection visible with `>` markers and keeps the keyboard cursor visible with `Cursor:`.
- Shows alert-bell messages as `[BELL]` and routing acknowledgements as `[OK]`.
- Keeps the bell sound for new direct alert-bell messages.
- Makes live layout commands harmless no-ops while the basic UI is active.

## 0.1.72 - 2026-07-25

- Fixes focused Chat so it is an internal destination instead of colliding with Radio Settings.
- Makes `A` in focused Chat open the reply keyboard.
- Draws the reply keyboard in both focused Messages and focused Chat.
- Makes the keyboard cursor obvious with a `Cursor:` status line and `>` marker.
- Collapses `Routing/ACK packet received` message rows to `[OK]` instead of noisy debug text.
- Improves Wii Remote pointer hover mapping for the reply keyboard based on the active focused panel.

## 0.1.71 - 2026-07-25

- Removes Chat from the left Dashboard Menu while keeping the right-side dashboard chat panel.
- Fixes the ticker to use the current ticker layout width instead of a hardcoded 48-character width.
- Shows the Wii IP address in the top bar when IP/UDP is active.
- Adds focused Messages selection and starts a reply composer with `A`.
- Adds a first on-screen reply keyboard with common words, letters, space, backspace, and send.
- Supports D-pad keyboard navigation and basic Wii Remote pointer hover selection.
- Queues UI direct replies through the existing Meshtastic serial send path.

## 0.1.70 - 2026-07-25

- Disables the previous dashboard slide transition because it felt like camera shake.
- Adds Chat as its own dashboard menu item and focused layout section.
- Adds node selection state for the focused Nodes screen.
- Lets focused Nodes use Up/Down to select a node, then `A` opens the focused Chat view for that node.
- Shows the selected node in the focused Nodes list with a `>` marker.

## 0.1.69 - 2026-07-24

- Splits dashboard card layouts from focused-window layouts.
- Adds `messages_focused`, `nodes_focused`, `map_focused`, `telemetry_focused`, and `radio_settings_focused` layout sections.
- Makes focused Wii windows use their own position, size, text size, title, and layer settings.
- Updates the layout editor focused preview to show/edit the focused section for each selected window.

## 0.1.68 - 2026-07-24

- Adds lightweight Wii dashboard window transitions when changing menu selection or focusing/returning from a window.
- Slides the dashboard bitmap text layer for a short eased transition while keeping USB diagnostics unchanged.
- Adds layout editor transition preview controls for checking enter-from-left/right spacing.

## 0.1.67 - 2026-07-24

- Adds dashboard browsing versus focused-window mode.
- Uses Up/Down to move the dashboard menu highlight while browsing, `A` to focus the selected window, and `B` to return to the full dashboard.
- Keeps the full dashboard showing all main panels while browsing; Radio Settings appears only when focused.
- Adds layout editor preview controls for active dashboard window and focused-window preview.

## 0.1.66 - 2026-07-24

- Treats blank section labels as no-title panels.
- Moves panel content up when a section label is blank, both in the Wii dashboard and layout editor preview.
- Makes Messages, Nodes, Chat, Telemetry, and placeholder panels use their configured labels instead of hardcoded titles.

## 0.1.65 - 2026-07-24

- Adds a `z` layer value to each `MeshLayout.config` section.
- Makes the layout editor draw sections by layer and bring the last-clicked section to the top.
- Saves/uploads/downloads section layer order over UDP.
- Draws Wii dashboard panel backgrounds in the saved layer order so overlapping blocks match the editor.

## 0.1.64 - 2026-07-24

- Tightens dashboard bitmap text spacing now that text is no longer using the old console layout.
- Reduces large placeholder/chat/radio-settings row gaps so the Wii view better matches the layout editor preview.
- Adds retrying UDP sends and longer timeouts to the layout editor and command-line layout uploader.

## 0.1.63 - 2026-07-24

- Adds a small bitmap dashboard text renderer so `text_size` affects on-Wii dashboard text.
- Keeps USB diagnostics on the fixed gxflux console renderer for dense debug output.
- Uses the same `MeshLayout.config` `text_size` values for editor preview and Wii dashboard rendering.

## 0.1.62 - 2026-07-24

- Moves dashboard text anchors inside their matching layout boxes instead of using border or fixed positions.
- Removes the hardcoded top-left `WiiMesh` title draw so topbar text follows the `topbar` layout box.
- Adds default-on editor snapping to the Wii 8x16 text grid so panels and console text move together predictably.
- Keeps the footer/help line anchored near the telemetry layout instead of a fixed screen row.

## 0.1.61 - 2026-07-24

- Adds `text_size` to every `MeshLayout.config` section.
- Adds a text-size field to the Python layout editor and uses it in the editor preview.
- Preserves `text_size` through Wii UDP upload/download/save so the layout file is ready for the future GX bitmap text renderer.
- Notes limitation: current Wii text still uses gxflux console glyphs, which do not support per-section scaling.

## 0.1.60 - 2026-07-24

- Creates `SD:/apps/wii-mesh/theme` on startup so live layout saves have a real target folder.
- Enlarges the UDP command buffer so layout text/sample fields are not truncated during live editor uploads.
- Adds editor-side live-drag/test-move/verify support for confirming that the running Wii build accepted the layout coordinates.
- Fixes the FTPii deploy script so running it from `outputs` uploads the boot.dol that was just built.

## 0.1.59 - 2026-07-24

- Fixes layout coordinate conversion so editor pixel positions are mapped through the Wii console overlay origin instead of raw screen origin.
- Rebuilds the built-in dashboard panel graphics from live `MeshLayout.config` boxes so text and panel outlines move together.
- Documents that current text is fixed-size console overlay text; per-section font scaling requires a future GX font renderer.

## 0.1.58 - 2026-07-24

- Adds live UDP layout editing commands so `MeshLayout.config` can be pushed without FTPii while WiiMesh is running.
- Adds UDP layout download/export support so the editor can fetch the layout currently used by the Wii.
- Expands the layout editor with Wii IP controls, UDP upload/download/reload buttons, and live sample-data import.

## 0.1.57 - 2026-07-24

- Adds Wii runtime loading for `SD:/apps/wii-mesh/theme/MeshLayout.config`.
- Adds UDP `LAYOUT` / `RELOAD_LAYOUT` to reload layout positions while WiiMesh is running.
- Adds `tools/upload_layout_ftpii.bat` so layout edits can be uploaded and reloaded after enabling IP/UDP with `+`.

## 0.1.56 - 2026-07-24

- Prevents the bell from playing for saved startup message history; it now arms after history is loaded and rings only for newly received direct alerts.
- Reduces dashboard text density so the default Dashboard page shows shorter Messages and Nodes summaries.
- Keeps the full message and node lists on their dedicated left-nav pages.
- Adds a Python/Tk layout editor that saves flat `MeshLayout.config` files for dashboard section positions and text.
- Updates the FTPii deploy script to upload `MeshLayout.config` when present.
- Improves the layout editor with filled draggable boxes, visible resize handles, multiline text editing, and realistic sample text previews.

## 0.1.55 - 2026-07-24

- Makes the left dashboard rail closer to the MeshControl reference, with WiiMesh branding, stacked navigation items, and a radio status card.
- Changes Up/Down to move through dashboard navigation items.
- Changes the top title to `WiiMesh` with the version shown separately.
- Adds an ASND-generated bell tone for direct notification-bell messages.

## 0.1.54 - 2026-07-24

- Replaces the dense debug-style main screen with a dashboard layout over the gxflux skin.
- Adds the requested left navigation labels: Dashboard, Messages, Nodes, Map, Telemetry, and Radio Settings.
- Adds a top bar with WiiMesh version, USB status, device name, and `Me:` node ID/nickname.
- Adds a top ACCESS ticker that cycles the latest received message text.
- Adds a right-side chat placeholder and a bottom telemetry placeholder while keeping real Messages and Nodes visible.

## 0.1.53 - 2026-07-24

- Adds a gxflux graphics-backed UI layer so WiiMesh is no longer plain console on a black screen.
- Adds a built-in dark Meshtastic-style dashboard skin with header, side rail, message panel, node panel, telemetry cards, and command bar shapes.
- Adds SD theme background support at `SD:/apps/wii-mesh/theme/background.rgb565` for custom visual skins without recompiling.
- Keeps the existing text UI over the new graphics layer so USB/message debugging remains readable while the GUI is upgraded.

## 0.1.52 - 2026-07-24

- Reworks the Wii text UI into a cleaner client-style layout with a compact status bar, calmer tab row, and newest-first content views.
- Improves the MSGS tab with sender/time rows, channel/node metadata, wrapped message text, and separators that are easier to read on a TV.
- Makes Stream and Debug newest-first so fresh traffic is immediately visible without scrolling.
- Keeps the detailed Debug tab off by default; press Minus to check/uncheck it when needed.
- Tightens USB diagnostics into a structured screen while preserving VID/PID/interface/endpoint details.

## 0.1.51 - 2026-07-24

- Shows newest messages first in the MSGS tab so freshly decoded texts are visible without scrolling.
- Stops adding firmware-console `TEXT_MESSAGE_APP` sightings as placeholder messages; those now stay in Stream/Debug until the framed PhoneAPI packet arrives.
- Keeps real decoded `TEXT_MESSAGE_APP` packets saving to `messages.dat` and appearing in MSGS.

## 0.1.50 - 2026-07-24

- Fixes USB receive truncation by reading Meshtastic serial chunks into a 512-byte buffer instead of 128 bytes.
- This should prevent large `FromRadio` config/node/message frames from being partially copied and then discarded.
- Keeps wake/session testing in place; current direct-message tests still use direct traffic only, not the shared channel.

## 0.1.49 - 2026-07-24

- Adds visible wake/resync mode logging for every config request.
- Adds UDP `WAKES` to list available wake/resync modes.
- Adds UDP `WAKE n` to select a wake/resync mode and immediately request config, so RAK serial startup can be tested from the PC without sending mesh traffic.

## 0.1.48 - 2026-07-24

- Adds Meshtastic-style pacing after serial writes so wake/config/heartbeat frames are not sent back-to-back faster than the RAK serial stack expects.
- Sends a PhoneAPI heartbeat after the connection is established, matching the Meshtastic Python client behavior that keeps the serial session alive.
- Renames the console-only fallback text so it no longer looks like a successfully decoded payload.

## 0.1.47 - 2026-07-24

- Changes serial wake/resync to match Meshtastic Python `SerialInterface`: writes 32 `0xc3` bytes before framed protobuf.
- Reverts normal startup to one full config request with a non-`69420` nonce, matching Python full-node mode.
- Removes the experimental forced `69420 -> heartbeat -> 69421` startup sequence from the Wii runtime.

## 0.1.46 - 2026-07-24

- Adds a fallback parser for RAK serial console lines that say `decoded message ... Portnum=1`.
- Creates a visible Messages entry and Debug card when the firmware console proves a text packet was decoded, even if no framed `FromRadio.packet` arrives.
- Clarifies Stream as the compact event feed and Debug as the single-card detail view.

## 0.1.45 - 2026-07-24

- Detects printable USB read chunks as RAK serial console/debug text.
- Shows ASCII previews for console text in Stream and Debug instead of hex-only rows.
- Includes both hex and ASCII previews in `RXUSB` file/UDP logs.

## 0.1.44 - 2026-07-24

- Decodes `FromRadio.metadata` enough to show firmware version strings instead of only raw hex.
- Adds Metadata debug cards for device metadata frames.
- Clarifies Debug tab numbering as `Card X/Y` and labels the card type in the header.

## 0.1.43 - 2026-07-24

- Logs every raw USB read chunk from the RAK as `RXUSB` before frame parsing.
- Adds `USB RX` Debug cards so incoming bytes are visible even when they do not form a valid framed `FromRadio`.
- Shows compact raw RX hex lines in Stream and raises Debug retention to 80 cards.

## 0.1.42 - 2026-07-24

- Logs every complete inbound framed `FromRadio` payload as `RXRAW` hex before parsing or filtering.
- Logs outbound wake/config/heartbeat/text frames as `TXRAW` hex.
- Adds display/log handling for more `FromRadio` variants: `log_record`, module config, queue status, metadata, and client notification.
- Parses `log_record` enough to show firmware debug messages as readable Debug cards.
- Improves serial frame resync when repeated `0x94` wake bytes or debug text appear in the stream.

## 0.1.41 - 2026-07-24

- Sanitizes Stream tab text so binary serial/protobuf bytes cannot render as garbage glyphs on the TV.
- Adds structured malformed-`FromRadio` stream entries with field/wire/length and a short hex preview.
- Keeps 30 parser stream events instead of 10 so config/channel startup traffic does not immediately evict useful packet logs.

## 0.1.40 - 2026-07-24

- Aligns serial startup with the current Meshtastic PhoneAPI reference: sends 4 wake bytes before the first framed protobuf.
- Switches from legacy one-shot `want_config_id=1` to the documented two-stage handshake: config nonce `69420`, heartbeat, then NodeDB nonce `69421`.
- Fixes `FromRadio.config` parsing by including field 5 in the accepted nested-message variants.

## PC serial helpers - 2026-07-24

- Adds Windows helper scripts to locate the RAK4631 USB serial port by VID/PID `239a:8029`.
- Adds a PuTTY launcher helper for the detected COM port at 115200 baud.
- Adds a Meshtastic CLI info collector that saves `--info` and `--nodes` output to `outputs/rak4631-pc-serial.log`.
- Adds a Meshtastic Python packet capture helper that writes parsed packet dicts to `outputs/meshtastic-packets.jsonl` for comparison with WiiMesh `RXRAW` logs.

## 0.1.39 - 2026-07-24

- Adds a fourth `DEBUG` tab modeled after the Meshtastic app debug panel.
- Stores packet debug cards with mesh summary, raw field layout, decoded data fields, port label, and decoded payload summary.
- Keeps Stream as the compact live log while Debug shows one detailed packet at a time with D-pad scrolling.

## 0.1.38 - 2026-07-24

- Shows successful UDP-originated direct/channel sends immediately in Messages as `[sent]` entries.
- Adds local Stream entries for Wii-to-RAK text sends, even if the RAK does not emit a serial ACK packet.
- Raises the main-app Stream retention to 30 stored events for easier scrolling during send tests.

## 0.1.37 - 2026-07-24

- Labels common Meshtastic port numbers in the Stream tab instead of showing only numeric ports.
- Treats `ROUTING_APP` port 5 packets as receipt/status entries in Messages so outgoing direct-message ACKs are visible.
- Keeps the `Text` counter limited to real `TEXT_MESSAGE_APP` packets.

## 0.1.36 - 2026-07-24

- Fixes the default channel display: empty Meshtastic primary channel names now render from the LoRa modem preset.
- Parses `FromRadio.config` LoRa settings and maps `LONG_FAST` to `LongFast`.
- Starts the UI with `LongFast` instead of the incorrect `Primary` fallback.

## 0.1.35 - 2026-07-24

- Shows IP/UDP status on the main screen instead of only inside USB diagnostics.
- Sends UDP command replies back to the PC, so `PING`, `DM`, `CH`, `CFG`, `CDC`, `MATRIX`, and `SCAN` report success or failure directly.
- Widens the UDP command buffer so longer direct/channel text test messages fit.

## 0.1.34 - 2026-07-24

- Adds test-only UDP text send commands after IP/UDP is manually enabled with `+`.
- Supports `DM !nodeid message` for direct text and `CH message` for Primary channel text.
- Encodes outbound `ToRadio.packet` with `MeshPacket.decoded` and `TEXT_MESSAGE_APP` payloads for Wii-to-RAK write testing.
- Updates the UDP helper script so multi-word text is sent without uppercasing the message body.

## 0.1.33 - 2026-07-24

- Stops initializing Wii IP/UDP networking during boot.
- Adds Wii Remote `+` as the manual trigger for UDP logging and UDP command socket setup.
- Shows the IP/UDP state on screen so USB testing can start without waiting on DHCP/network setup.

## 0.1.32 - 2026-07-24

- Logs every received MeshPacket to the Stream tab and UDP before filtering for text.
- Adds packet and decoded-packet counters.
- Logs MeshPacket field/wire summaries, decoded payload size, port number, channel/direct direction, and encrypted/no-decoded cases.

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
