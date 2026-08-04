# Changelog

All notable WiiMesh test-build changes are tracked here so real-Wii testing does not depend on chat history.

## 0.1.184 - 2026-08-03

- Replaces the global incoming-message seen counter with per-message seen state.
- Opening a chat now clears unread state only for that chat instead of clearing every unread incoming message.
- Loads saved `messages.dat` entries as already seen so long-running or rebooted sessions do not replay old bell alerts.
- Fixes the visible `Text` count/live-data count to use the actual saved message list after loading history.
- Adds unread count to `LIVE_DATA` replies for long-run debugging over UDP.
- Live UDP check on `192.168.0.42` showed the test text was parsed and saved (`DM Mesheteer | testing | 66 saved`), pointing this fix at UI unread/chat filtering rather than USB receive failure.
- Documents the manual direct-message MIDI transfer format so custom audio clips can be sent in chunks and auto-played by WiiMesh.

## 0.1.183 - 2026-08-02

- Replaces the bare chat-detail range counter with a clearer `MSG X-Y OF Z` label.
- Leaves chat-list right-side values as timestamps for the latest activity in each chat.

## 0.1.182 - 2026-08-02

- Restores normal chat ordering so the newest message appears at the bottom of the conversation.
- Keeps `Up` scrolling toward older messages and `Down` returning toward newer messages.
- Applies the same oldest-to-newest order to the text fallback message/chat screens.

## 0.1.181 - 2026-08-01

- Plays the notification sound for any new incoming message, matching the unread bell icon.
- Keeps sound silent for outgoing sends, MeshFile receipts, and saved-history loading at boot.

## 0.1.180 - 2026-08-01

- Splits the graphical header into fixed title, radio status, and Wii IP slots so `ONLINE` and the IP/UDP text no longer overlap.
- Gives the ticker its own width with room reserved for the signal bars.
- Separates the text-mode fallback header status and IP/UDP labels so they do not print over each other.

## 0.1.179 - 2026-08-01

- Fixes notification bell semantics so the icon only appears for unread incoming messages.
- Stops outgoing sends, local messages, and MeshFile receipt replies from creating unread bell state.
- Plays the bell sound only for incoming direct bell-alert messages, not messages sent by WiiMesh.
- Updates home/chat/screensaver unread counts to use incoming message counts instead of total saved messages.

## 0.1.178 - 2026-08-01

- Adds a delete action to the MIDI player using the Wii Remote `-` button.
- Deletes only the selected `.mid`/`.midi` file from `SD:/apps/wii-mesh/received_files`, stops playback first when deleting the currently playing file, and refreshes the file list afterward.
- Updates graphical and text MIDI player help to show `- DELETE`.

## 0.1.177 - 2026-08-01

- Renders active-user screensaver entries as graphical badges with avatar/icon glyphs instead of console text.
- Stops printing cleaned fallback labels like `[ICON]` over screensaver badges.
- Uses the node-list icon extraction path for screensaver badges so emoji short names can display as real badge graphics.

## 0.1.176 - 2026-08-01

- Removes the filled screensaver text box in username/latest-message modes so the rectangle no longer appears to lag behind character-cell text.
- Keeps the animated signal bars on the screensaver while making the moving text itself float directly on the background.

## 0.1.175 - 2026-08-01

- Adds explicit MeshFile receipt replies from WiiMesh: `[MFACK] filename START`, `[MFACK] filename CHUNK n/total`, and `[MFACK] filename END COMPLETE/MISSING`.
- Uses those app-level replies to prove the Wii parsed each transfer stage instead of relying only on Meshtastic Web delivery icons.
- Keeps MeshFile ACK replies direct-only and separate from public channel chats.

## 0.1.174 - 2026-08-01

- MeshFile transfer assembly now falls back to matching active transfers by filename when Meshtastic reports sender identity differently across direct START/CHUNK/END texts.
- Trims `[END]` filenames before matching so CR/LF or trailing spaces do not create a separate empty transfer.
- Live UDP testing showed direct MeshFile control messages arrive from Chrome/Meshtastic Web, but v0.1.173 left transfers at `0/0 receiving`; this patch targets that merge failure.

## 0.1.173 - 2026-08-01

- Fixes the Settings panel so compact/custom layouts scroll through all Settings rows instead of hiding the bottom entries.
- Makes the `WII IP` row visible/selectable again from Settings, keeping IP/UDP disabled at boot and off the global `+` shortcut.
- Updates pointer Settings selection to match the currently visible scrolled Settings rows.

## 0.1.172 - 2026-08-01

- Adds a Settings -> GUI Options page for enabling/disabling side-menu buttons.
- Persists menu button visibility, font settings, screensaver settings, debug toggle, pointer toggle, and MIDI repeat in `GUI.config`.
- Keeps old `GUI.config` placement files compatible; missing option keys default to the current visible menu.
- Makes side-menu D-pad and pointer navigation skip disabled buttons.
- Wires UDP layout reload/set/save back into the active `GUI.config` path.

## 0.1.171 - 2026-08-01

- Adds a visible `MIDI / FILES` Settings row that opens the MIDI player and saved MeshFile transfer folder without enabling Wii IP.
- Shows MeshFile and MIDI status directly on the Home panel even when the separate MIDI card is hidden by a compact layout.
- Adds MIDI/Files status to the text fallback Settings screen.

## 0.1.170 - 2026-08-01

- Stops the Wii network/IP/UDP stack from starting from the global `+` shortcut.
- Keeps IP/UDP fully off at boot unless the Settings Wii IP action is selected.
- Updates footer/status text so testers know Wii IP is enabled from Settings instead of the main UI.

## 0.1.169 - 2026-08-01

- Fixes the on-screen keyboard radio badge key to insert the real Unicode `U+1F4FB` UTF-8 character instead of the visible `:radio:` fallback text.
- Adds Unicode-aware compose cursor movement and Backspace so badge characters are removed as one logical character.
- Draws the compose preview with the existing Unicode badge renderer so supported badges appear as graphics.
- Reworks keyboard rows so suggestions no longer overlap control keys.
- Keeps one visible wide `Space` key and moves Shift, Caps, Mode, cursor, Back, and Enter into the bottom control row.

## 0.1.168 - 2026-08-01

- Fixes the keyboard bug where normal letter keys were inserted as word suggestions with automatic spaces.
- Adds typed keyboard actions so characters, suggestions, emoji badges, Space, Backspace, and Enter use separate code paths.
- Starts the keyboard in lowercase mode and adds mode cycling: `abc -> ABC -> 123 -> Emoji`.
- Adds Shift, Caps, symbols, cursor left/right, Clear, Cancel, Backspace, and Enter keys.
- Reuses the existing emoji badge/icon rendering and inserts badge alias tokens without automatic spaces.
- Makes Backspace and cursor movement treat inserted badge aliases as one logical item.

## 0.1.167 - 2026-07-31

- Changes the compose keyboard to a standard QWERTY-style layout.
- Keeps only one top suggestion row for most-used words.
- Removes the extra default-word rows so letters remain visible.
- Makes suggestion keys wider and skips spacer cells during D-pad/pointer navigation.

## 0.1.166 - 2026-07-31

- Adds a Home-screen MIDI player card for saved MeshFile MIDI transfers.
- Adds a MIDI player submenu opened from Home with `2`.
- Supports selecting saved `.mid`/`.midi` files, `A` to play, `1` to stop, and `2` to toggle repeat.
- Scans `sd:/apps/wii-mesh/received_files/` for MIDI files and updates after new MeshFile saves.
- Persists the MIDI repeat setting in `Settings.config`.

## 0.1.165 - 2026-07-31

- Restricts MeshFile transfer commands to direct messages only; channel/Main-chat transfer commands are logged and ignored.
- Adds base64 MeshFile chunk support for binary test files such as MIDI.
- Saves decoded binary transfers to `sd:/apps/wii-mesh/received_files/`.
- Adds a small MIDI parser/synth path that autoplays completed `.mid`/`.midi` transfers on Wii audio.
- Adds a PC helper to generate a tiny MIDI and the exact direct-chat MeshFile strings for Chrome testing.

## 0.1.164 - 2026-07-31

- Adds receive-side MeshFile text-transfer support using `[START]`, `[CHUNK]`, and `[END]` messages from `VeggieVampire/MeshFile`.
- Queues per-chunk confirmation texts back to the sender.
- Saves completed transfers under `sd:/apps/wii-mesh/received_files/`.
- Shows compact MeshFile status in Home/live layout data and keeps large chunk payloads compact in chat.

## 0.1.163 - 2026-07-31

- Changes the keyboard top row from most-recent words to most-used words.
- Counts words from sent text and saved chat messages, then ranks them by frequency for the shortcut row.
- Keeps the normal letter keyboard below the single most-used-words row.

## 0.1.162 - 2026-07-31

- Limits recent keyboard words to only the first keyboard row.
- Restores the normal `A` through `Z` keys below the recent-word row.
- Fixes the keyboard builder so one-letter keys are no longer filtered out as invalid words.

## 0.1.161 - 2026-07-31

- Fixes open direct-chat scrolling so `Up` moves to older messages and `Down` returns toward newer messages.
- Adds a visible chat range counter like `1-4/7` in chat detail to confirm how many messages the active conversation matched.
- Uses one shared visible-count helper for chat detail drawing and D-pad scroll limits.

## 0.1.160 - 2026-07-31

- Stops chat names/messages from showing the literal `[icon]` placeholder when a Meshtastic short name contains an emoji/icon.
- Draws the selected direct-chat node badge in the chat header so the contact icon appears as graphics instead of text.
- Cleans node-list-to-chat naming so emoji-only short names fall back to long name or node ID for text labels.

## Script update - 2026-07-31

- Adds curl FTP data-transfer timeouts to deploy and log-fetch scripts so FTPii passive data socket hangs fail instead of freezing indefinitely.
- Disables EPSV for FTPii because FTPii reports `502 Command not implemented` before falling back to PASV.
- Adds optional `--active` mode to deploy/fetch scripts for testing active FTP if passive PASV data ports keep hanging.

## 0.1.159 - 2026-07-31

- Adds a recent-word keyboard list built from sent text and recent received messages.
- Places recently used words before canned words and letter keys instead of limiting the top row to a few fixed shortcuts.
- Adds keyboard paging hints and keeps pointer hit boxes aligned with the currently visible key page.

## 0.1.158 - 2026-07-31

- Restores Meshtastic user badges on inactive direct chats by reusing the Node List avatar renderer in the Chats list.
- Keeps inactive chat badges visible with a dimmed border instead of dropping back to a plain/no badge look.

## 0.1.157 - 2026-07-31

- Fixes active direct-chat naming so the open chat prefers the known Meshtastic long name instead of showing a raw `!nodeid`.
- Uses short name as the second fallback when opening a chat from the node list.
- Replaces the direct-chat header's raw node ID label with `DIRECT` so the contact name area stays readable.

## 0.1.156 - 2026-07-31

- Adds scrolling inside the open chat detail view when a conversation has more than four messages.
- Shows `NEWER ^` and `OLDER v` hints inside chat detail when messages are hidden.
- Restores newest-at-top ordering for the chat detail view.
- Sorts the main Chats list by latest activity and shows `MORE ^` / `MORE v` when hidden chat rows exist.

## 0.1.155 - 2026-07-31

- Fixes chat bubble ordering so the newest message appears at the bottom.
- Bottom-aligns the visible chat stack like a normal conversation view.

## 0.1.154 - 2026-07-31

- Adds explicit outgoing and delivered state to saved messages.
- Draws local sent chat bubbles with a gray check mark while pending.
- Marks the latest matching outgoing message delivered when a Meshtastic routing/status packet arrives, turning the check green.

## 0.1.153 - 2026-07-31

- Adds hold-to-repeat for Wii Remote D-pad movement after a short delay.
- Applies repeat movement to menu navigation, list scrolling, node/chat selection, keyboard cursor movement, and placement-editor move/resize controls.
- Keeps action buttons one-shot so holding `A`, `B`, `1`, `2`, or `HOME` does not spam actions.

## 0.1.152 - 2026-07-31

- Fixes Wii UI keyboard direct-message sends so successful sends immediately add a local `[sent]` chat bubble.
- Marks UI-sent messages dirty so they are saved to `messages.dat` like UDP-sent messages.
- Confirms live `0.1.151` receive state over UDP showed real direct text from `MacnCheeseNoodles` again.

## 0.1.151 - 2026-07-31

- Restores the old working receive rule from `0.1.51`: firmware-console `TEXT_MESSAGE_APP` sightings stay in Stream/Debug only until a real framed payload arrives.
- Filters saved routing ACK/status rows out of `messages.dat` on load/save so old ACK pollution no longer appears as chat.
- Fixes the UDP command helper so `LIVE_DATA` replies containing emoji/UTF-8 text print correctly on Windows.

## 0.1.150 - 2026-07-31

- Fixes send/receive separation for serial console fallback text packets.
- Stops outbound phone/Wii-side `TEXT_MESSAGE_APP` console notices from being counted as received messages.
- Keeps routing ACK packets in debug/status instead of adding fake chat rows to the message list.

## 0.1.149 - 2026-07-31

- Enlarges the chat keyboard keys for Wii Remote pointer use and D-pad navigation.
- Changes the chat keyboard from 7 narrow columns to 6 wider columns.
- Keeps pointer hit boxes matched to the drawn key boxes.

## 0.1.148 - 2026-07-31

- Fixes Wii Remote pointer clicks on the chat keyboard so clicking a visible key presses that key instead of the old D-pad cursor key.
- Gives pointer keyboard clicks their own status text for debugging key hit boxes.

## 0.1.147 - 2026-07-31

- Restores a visible message fallback when Meshtastic firmware logs a decoded `TEXT_MESSAGE_APP` but no framed payload follows.
- Makes direct chat detail matching accept either sender or recipient node ID so saved direct messages do not disappear from the open chat.
- Keeps the larger node/chat badges from 0.1.146.

## 0.1.146 - 2026-07-31

- Doubles Node List badge/card size so Meshtastic user badges are readable, with fewer rows and normal scrolling.
- Enlarges direct chat badges and chat rows so chat identities match the node list better.
- Resolves direct chat list and opened chat names through known node long names instead of showing node IDs when node info is available.

## 0.1.145 - 2026-07-31

- Moves dashboard menu labels into a full 16px text lane so `NODE` no longer clips into `NUDE`.
- Moves graphical section underlines below the libogc 8x16 font instead of through the letters.
- Gives Settings, Font, Screensaver, and Node Options rows enough vertical room for the fixed console font.

## 0.1.144 - 2026-07-31

- Stops regenerating the UI text font during normal builds.
- Keeps the ftpii/libogc-style console font as a fixed checked-in WiiMesh asset.
- Leaves build-time generation only for dashboard/menu badges and Meshtastic emoji badge atlases.

## 0.1.143 - 2026-07-31

- Switches the WiiMesh graphical text atlas to libogc's 8x16 console font, matching the font approach used by ftpii.
- Replaces the Windows TrueType rounded font generator with a console-font generator so glyphs are fixed-width and TV-safe.
- Keeps the Font Debug sheet for checking the exact ftpii-style character set on real hardware.

## 0.1.142 - 2026-07-31

- Rebuilds the rounded UI font with a clearer rounded source font and more glyph spacing.
- Removes the extra blur pass that made small characters smear together on TV output.
- Keeps the Font Debug sheet for checking every printable character on real Wii hardware.

## 0.1.141 - 2026-07-31

- Adds a Placement Editor entry under Settings so GUI placement is reachable without remembering the `-` shortcut.
- Adds a Font Debug sheet that displays printable ASCII characters using the active font style and size.
- Preserves lowercase and printable punctuation in the rounded UI font renderer for better photo-based glyph tuning.

## 0.1.140 - 2026-07-31

- Rebuilds the graphical UI font as a generated rounded anti-aliased bitmap atlas.
- Replaces square block text rendering with alpha-blended rounded glyphs for Wii graphics screens.
- Adds `outputs/rounded_font_preview.png` during builds so the font can be checked outside the Wii.

## 0.1.139 - 2026-07-31

- Adds live `sd:/apps/wii-mesh/Settings.config` support for font, screensaver, debug, and pointer settings.
- Adds UDP commands `SETTINGS_GET`, `SETTINGS_SET key=value`, `SETTINGS_RELOAD`, and `SETTINGS_SAVE`.
- Loads `Settings.config` on boot and creates it with defaults when missing.
- Updates FTPii fetch/deploy helpers to fetch and preserve `Settings.config`.

## 0.1.138 - 2026-07-31

- Fixes Screensaver Test immediately closing when the A button is still held from selecting it.
- Adds screensaver start/dismiss details to `debug.log`, including active state, idle counter, dismiss block, mode, and speed.
- Enables Wii Remote IR tracking, logs pointer position in heartbeat lines, and draws a small crosshair cursor in the graphical UI.

## 0.1.137 - 2026-07-31

- Replaces the three-step font size picker with a 0-6 slider for finer TV tuning.
- Adds intermediate bitmap font scaling so text can be larger without jumping straight to huge 2x pixels.
- Adds Pixel, TV Shadow, Soft Pixel, and Bold font render styles.
- Logs active font style and size when changed and in heartbeat lines.

## 0.1.136 - 2026-07-31

- Adds UI tab breadcrumbs to `debug.log`, including selected node count and map revision.
- Logs a matching `UI draw ok` line after the first draw on a newly entered tab so crashes during drawing are easier to identify.

## 0.1.135 - 2026-07-31

- Fixes a Map tab freeze when the selected node index is outside the first plotted node set.
- Clamps the map line anchor so large node lists cannot read past the plotted coordinate arrays.

## 0.1.134 - 2026-07-31

- Renames the Meshtastic hardware IP label from `RAK IP` to `M Device IP` so future non-RAK devices fit the UI.

## 0.1.133 - 2026-07-31

- Separates Wii LAN IP, Meshtastic device IP, and UDP log target in the UI.
- Displays Wii IP in the graphical header and Home/Status screens.
- Learns the RAK/Meshtastic device IP from network-related serial console lines when the radio reports one.

## 0.1.132 - 2026-07-31

- Splits Screensaver and Font controls into their own Settings submenus.
- Adds a Screensaver test action so the saver can be started immediately from Settings.
- Keeps the main Settings screen focused on logs/status, submenus, and network.

## 0.1.131 - 2026-07-31

- Adds more inner padding to Node List badges so custom icons do not touch the circular border.
- Shrinks badge artwork slightly and moves node text farther right for better spacing.

## 0.1.130 - 2026-07-31

- Adds Settings rows for live graphical font style and font size testing on the Wii.
- Supports Clean, Shadow, and Bold Test styles plus Small, Normal, and Large size choices.

## 0.1.129 - 2026-07-31

- Fixes the over-bold graphical font from 0.1.128 that merged letters together on the Wii.
- Keeps text contrast shadowing while restoring clean one-pixel glyph strokes.

## 0.1.128 - 2026-07-31

- Makes the graphical UI font heavier and easier to read on a real TV.
- Adds subtle text shadowing and wider glyph strokes for better contrast.

## 0.1.127 - 2026-07-31

- Slows the screensaver default movement.
- Adds Settings controls for screensaver display mode and speed.
- Lets the screensaver show the node name, latest unread message, or active users summary.
- Active Users screensaver mode now gives each visible user its own bouncing label box.

## 0.1.126 - 2026-07-30

- Changes the `CHAT` tab into an active chats list like Meshtastic UI.
- Opens selected channels and direct contacts into a dedicated threaded chat view.
- Adds unread highlighting and channel/direct chat target state so channel conversations no longer live in the channel list.

## 0.1.125 - 2026-07-30

- Restores the Channels tab to the Meshtastic UI model: a list of configured channels with key/lock status.
- Makes `A` on a selected channel open that channel in the Messages/Chats view.
- Keeps channel configuration detail out of the main Channels screen.

## 0.1.124 - 2026-07-30

- Moves channel chat history to the Chats/Messages tab where Meshtastic Web shows `Messages: Primary`.
- Restores the Channels tab to channel configuration/settings only.
- Adds a compact contacts rail to the Chats tab so channel conversation and available nodes live together like Meshtastic Web.

## 0.1.123 - 2026-07-30

- Adds selected-channel chat history to the Channels tab after comparing Meshtastic Web's `Messages: Primary` view in Chrome.
- Keeps channel settings compact at the top and shows recent non-direct messages for the selected channel underneath.
- Adds emulator mock channel traffic so the Channels tab can be visually checked without live radio packets.

## 0.1.122 - 2026-07-30

- Updates the Channels tab after comparing it in Dolphin with Meshtastic Web in Chrome.
- Changes Channels from a list-only view to a Meshtastic-style channel selector row: `Primary`, `Ch 1` through `Ch 7`.
- Shows a selected-channel settings panel with role, PSK/key state, name, uplink/downlink, location precision, and muted status.

## 0.1.121 - 2026-07-30

- Reworks the Channels tab into graphical Meshtastic-style channel rows instead of placeholder pills.
- Parses channel role, PSK/key presence, channel ID, uplink/downlink flags, muted state, and position precision from channel config frames.
- Adds Up/Down selection inside the Channels tab after pressing `A`.
- Adds debug/mock channel rows so emulator builds can preview primary, secondary, and open channel states.

## 0.1.120 - 2026-07-30

- Adds a first Wii-side plain-HTTP map tile downloader for internet-connected consoles.
- Reads the tile URL template from `sd:/apps/wii-mesh/maps.url` and supports `{z}`, `{x}`, and `{y}` placeholders.
- Adds UDP commands `TILE_URL`, `TILE_SET_URL`, and `TILE_GET z x y [style]`.
- Saves downloaded PNG tiles under `sd:/apps/wii-mesh/maps/<style>/<z>/<x>/<y>.png`, matching Meshtastic UI's tile folder shape.
- Adds PC helper BAT files for setting the tile URL and downloading a single test tile over UDP.

## 0.1.119 - 2026-07-30

- Parses Meshtastic `POSITION_APP` packets and NodeInfo embedded positions into per-node latitude, longitude, altitude, and precision fields.
- Changes the Map screen to prefer real GPS positions when available, while keeping the relative fallback for nodes without coordinates.
- Saves downloaded map data to `sd:/apps/wii-mesh/mesh_map.dat` and adds UDP `MAP_GET`/`MAP_SAVE` commands.
- Adds `tools/download_map_udp.bat` for pulling the live mesh map from the running Wii without FTP.
- Updates the FTP log fetcher to pull `mesh_map.dat` when it exists.

## 0.1.118 - 2026-07-30

- Replaces the Map placeholder with a graphical relative mesh view using known Meshtastic nodes.
- Draws stable node badge positions, mesh connection lines from this device, selected-node highlighting, and a node summary strip.
- Keeps Map controls consistent with Node List: Up/Down changes the selected node and `A` opens chat with that node.

## 0.1.117 - 2026-07-30

- Moves the default graphical layout to the tuned Wii-style positions: left menu, compact header strip, wider content panel, and full-width footer.
- Compacts older tall-header `GUI.config` layouts at runtime by shrinking the header and moving/expanding content upward, without writing over the saved config file.

## 0.1.116 - 2026-07-30

- Reduces the graphical header from a large duplicate dashboard card into a compact status strip.
- Removes the repeated USB, Me, and RX rows from the graphical header because that detail already lives on Home/status screens.
- Keeps the header focused on WiiMesh/version, online/IP state, channel/message counts, ticker text, and signal bars.

## 0.1.115 - 2026-07-30

- Shrinks graphical screen titles from large two-scale text to compact one-line labels.
- Moves Home, Node List, Messages, Chat, and Node Options content upward to recover vertical space for cards and the compose keyboard.

## 0.1.114 - 2026-07-30

- Reworks the Messages screen into graphical message cards with a channel header, selected-card highlight, and clipped sender/message previews.
- Adds a graphical direct-message compose keyboard inside Chat Detail so composing no longer drops back to the console-style text keyboard.
- Wraps chat bubble text to stay inside the chat window and keeps the keyboard visible with D-pad selection, `A` to press a key, and `B` to cancel.
- Fixes the message selection window so highlighted messages and `A` open/reply target the same message.

## 0.1.113 - 2026-07-30

- Uses the live Meshtastic Web node table as the Node List reference: avatar badge, long name, connection, SNR/last heard, model, and MAC-derived fallback.
- Parses more `NodeInfo` protobuf fields: `snr`, `last_heard`, `device_metrics`, and `hops_away`.
- Parses `User.macaddr` alongside `short_name` and `hw_model` so node cards have better identity/status detail.
- Updates the Dolphin icon-debug sample data to exercise real-looking Meshtastic node cards without USB hardware.

## 0.1.112 - 2026-07-30

- Moves the normal Home, Node List, Channels, Chats, Map, Settings, Chat Detail, and Node Options screens to the graphics layer so the large console text no longer overlays the UI.
- Reworks the Node List into Meshtastic-style cards with bordered avatar badges, long names, node IDs/short-name fallback, selected-row highlight, and right-side hardware/status text.
- Keeps menu labels below the menu icons and clips all graphics-layer labels to their boxes to reduce text spill on TV output.
- Parses the Meshtastic `User.hw_model` field from node info so nodes can show hardware labels such as `RAK4631`, `HELTEC_V3`, and `HELTEC_V4`.
- Adds a graphical Node Options screen with Filter and Highlight tabs, preserving `A` to go deeper/focus and `B` to go back.

## Tools - 2026-07-29

- FTPii deploy now prefetches the remote theme folder listing and `theme/MeshLayout.config` before uploading.
- FTPii deploy preserves Wii-side `GUI.config` and `theme/MeshLayout.config` by default.
- `MeshLayout.config` upload is now opt-in with `--upload-layout`; theme background upload is opt-in with `--upload-theme`.
- FTPii log fetch now also pulls remote listings, `messages.dat`, `GUI.config`, and `theme/MeshLayout.config`.

## 0.1.111 - 2026-07-30

- Changes Meshtastic node emoji badges from 1-bit masks to 32x32 RGB565 pixel icons with per-pixel alpha.
- Adds `outputs/emoji_atlas_preview.png` generation so emoji glyphs can be checked outside Dolphin before building.
- Draws embedded emoji atlas pixels directly in the Wii Node List badge for closer one-to-one comparison with desktop Meshtastic.

## 0.1.110 - 2026-07-30

- Adds a build-time emoji icon atlas generated from the Windows emoji font for Meshtastic node short-name icons.
- Renders the known `U+1F93A` Mesheteer short-name emoji as a bitmap badge instead of a generic hand-drawn fallback.
- Aligns graphical Node List badge rows with the console text rows so badges stay beside the correct node names.
- Adds a compile-time `WIIMESH_ICON_DEBUG` Dolphin test mode for icon comparison without USB hardware.

## 0.1.109 - 2026-07-30

- Removes redundant short-name text from Node List rows when the badge already represents the short name/icon.
- Node List rows now show badge plus readable long name, with node ID/status on the second line.

## 0.1.108 - 2026-07-30

- Changes Node List avatars from square tiles to circular badges with a visible ring border, closer to Meshtastic web/PC node lists.
- Enlarges single custom icon drawing to fill the badge more clearly.
- Adds fixed pixel masks for person/running-style short-name icons commonly used as Meshtastic node badges.

## 0.1.107 - 2026-07-30

- Avatar/icon detection now checks Meshtastic `short_name` first, then `long_name`, matching how node badges are usually presented.
- Enlarges Node List avatar tiles to 40x40 and adds a clear double border around both custom icons and short-name badges.
- Custom multi-codepoint node icons are drawn larger as pixel masks in the badge instead of being tiny row markers.
- Node diagnostics now log both long-name and short-name UTF-8 summaries for icon debugging.

## 0.1.106 - 2026-07-30

- Node badge colors are now stable per node identity/short name instead of being based on row position.
- Matches Meshtastic's client-side color behavior more closely: `User` supplies names/identity, while WiiMesh derives the avatar color locally.
- Keeps short-name badges consistent when sorting changes or newly discovered nodes arrive.

## 0.1.105 - 2026-07-30

- Node List now uses the Meshtastic short name field when available.
- Nodes without custom icon/emoji codepoints now draw a colored round badge with a compact short-name label instead of the generic radio icon.
- Badge fallback uses the short name first, then a cleaned long-name/ID fallback such as `3618`, `CC94`, or `TULL`.

## 0.1.104 - 2026-07-30

- Draws the fetched custom node-name symbols as fixed pixel masks instead of compressed line glyphs.
- Prioritizes nodes with custom UTF-8 icon codepoints at the top of the Node List so they are visible during testing.
- Keeps the fetched Wii `GUI.config` preserved; this build changes rendering only.

## 0.1.103 - 2026-07-30

- Uses the fetched real node-name UTF-8 evidence (`U+1F426 U+200D U+2B1B U+1F99E U+1F5FC`) to render multiple custom avatar glyphs in one Node List icon tile.
- Adds explicit Wii-side glyphs for the received bird, lobster, and tower codepoints, while skipping ZWJ/black-square sequence glue.
- Node List diagnostics now summarize recognized icon sequences instead of only the first codepoint.

## 0.1.102 - 2026-07-30

- Adds UTF-8 node-name diagnostics so `debug.log` reports emoji/codepoint values like `U+1F4E1` when Meshtastic node names include icons.
- Adds GitHub-style colon alias recognition such as `:house:`, `:satellite:`, `:bell:`, `:lock:`, `:car:`, and related groups.
- Temporarily shows recognized node icon codepoints on the Node List as `icon U+...` to confirm what WiiMesh is seeing from the radio.

## 0.1.101 - 2026-07-30

- Expands the WiiMesh custom emoji/avatar icon font using the GitHub emoji cheat sheet groups as a guide.
- Adds grouped pictogram support for faces, people, weather, plants, creatures, devices, radio, locks, messages, map pins, vehicles, media, tools, mail/docs, hearts, stars, alerts, and questions.
- Skips variation selectors and skin-tone modifiers when picking the first icon from a Meshtastic node name.

## 0.1.100 - 2026-07-30

- Starts a real WiiMesh-owned emoji/avatar icon font for Meshtastic user names instead of relying on the Wii console font.
- Node cards now parse UTF-8 emoji/icon codepoints from node names and draw matching Wii-side pictograms for home, radio, bell, chat, map, key, lock, nodes, mobile, signal, and unknown icons.
- Keeps text fallback tags for screens that still use console text, while Node List gets graphical avatar rendering.

## 0.1.99 - 2026-07-30

- Reworks Node List into Meshtastic-style stacked node cards with colored radio glyphs.
- Shows each node with a short display label, full/long name, node ID-driven selection, and compact right-side status text.
- Keeps `A` for opening chat, Up/Down for node selection after focus, and `2` for Node Options.

## 0.1.98 - 2026-07-30

- Tightens text clipping so content/footer text respects the actual configured panel width.
- Shortens the Home dashboard into a calmer summary view with fewer visible rows.
- Reduces footer/help text length to avoid fighting the lower screen panel.

## 0.1.97 - 2026-07-30

- Adds focus mode for primary menu screens: Up/Down moves the menu until `A` is pressed, then Up/Down scrolls/selects inside the active screen and `B` returns to menu navigation.
- Pads every console-rendered line to its allowed width so old text does not remain outside panels when shorter text redraws.
- Adds graphical row/card backgrounds and a focused content outline to make the interface read more like an app dashboard.
- Makes the Home dashboard scrollable instead of overflowing below the content window.

## 0.1.96 - 2026-07-30

- Adds a 7-minute idle screen saver based on the earlier Mvskoke Party Wii behavior.
- Screen saver shows the Meshtastic device/user name, or node ID/WiiMesh fallback, in a bouncing box to reduce burn-in risk.
- Any Wii Remote button wakes the screen saver; HOME exits only after the saver is dismissed.

## 0.1.95 - 2026-07-30

- Adds UTF-8 emoji/icon cleanup for node names, sender names, messages, ticker text, and debug previews.
- Maps common Meshtastic-style name icons to Wii-safe tags such as `[home]`, `[radio]`, `[bell]`, `[chat]`, `[map]`, `[lock]`, `[key]`, `[signal]`, and `[mobile]`.
- Falls back unknown emoji/avatar glyphs to `[icon]` instead of drawing raw UTF-8 garbage on the Wii console font.

## 0.1.94 - 2026-07-30

- Splits the node area into a Meshtastic-style Node List and a separate Node Options view.
- Node List now shows online/known node counts, selectable node rows, node IDs, names, and chat availability.
- Adds a Filter/Highlight Node Options placeholder page reachable with `2` from Node List.
- Keeps custom user PNG menu icons compiled into `boot.dol` during normal builds.

## 0.1.93 - 2026-07-30

- Reworks the Home submenu into a Meshtastic UI-style status list with messages, online nodes, time/date, radio/channel, signal, power, air utilization, bell, GPS, network, and build rows.
- Parses RAK serial-console telemetry/status lines for online node count, battery, voltage, channel utilization, air utilization, SNR, and RSSI when firmware logs provide them.
- Keeps the fetched/saved Wii-side `GUI.config` untouched.

## 0.1.92 - 2026-07-30

- Rebuilds `boot.dol` with edited icon PNGs from `Z:/art/AI/Wii icons`.
- Maps the source `setting.png` file into WiiMesh's expected `settings.png` icon slot.

## 0.1.91 - 2026-07-30

- Adds editable PNG assets for every dashboard menu icon and the unread bell.
- Compiles `assets/ui_icons/*.png` into `include/wiimesh/generated/MenuIcons.h` during `make`.
- Adds `tools/build_with_output_icons.bat` so edited `outputs/ui_icons/*.png` files can be synced and built into `boot.dol`.
- Updates FTPii deploy so normal uploads sync edited `outputs/ui_icons/*.png` before compiling `boot.dol`.

## 0.1.90 - 2026-07-30

- Shrinks the dashboard menu icons so labels have more room below them.
- Makes the `menu` zone height control vertical icon spacing in the Wii placement editor.
- Keeps the fetched/saved `GUI.config` untouched while changing the built-in menu rendering.

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
