# WiiMesh Layout Editor

Run the editor from Windows:

```bat
tools\mesh_layout_editor.bat
```

It creates or edits:

```text
MeshLayout.config
```

The file is a flat `key=value` config. The intended Wii path is:

```text
SD:/apps/wii-mesh/theme/MeshLayout.config
```

The editor uses a 640x480 canvas. Drag boxes to move UI sections. Shift-drag a
box to resize it. Arrow keys nudge the selected section. Shift plus arrow keys
resize it.

Keep `Snap text grid` enabled when matching the Wii. The current Wii text layer
draws on an 8x16 character grid, so rectangles can move pixel-by-pixel but text
only moves when a section crosses the next text cell.

`text_size` changes the preview font size in the editor and the on-Wii dashboard
text. USB diagnostics still use the fixed console font because that screen is
temporary dense debug output.

Sections currently included:

- `topbar`
- `ticker`
- `sidebar`
- `radio_status`
- `messages`
- `nodes`
- `map`
- `chat`
- `telemetry`

This tool prepares the config file; WiiMesh runtime support for loading
`MeshLayout.config` can reload it while the app is open.

## UDP Upload, Download, And Live Data

1. Press `+` in WiiMesh so IP/UDP is enabled.
2. Make changes in the editor.
3. Use the editor buttons:
   - `Upload Live` sends the layout to WiiMesh over UDP and saves it on SD.
   - `Download` pulls the current Wii layout over UDP.
   - `Import Data` pulls current message/node/status text for preview.
   - `Reload Wii` reloads the SD config.
   - `Test Move Map` sends only the map box coordinates. The map panel should visibly jump left on the Wii; if it does not, confirm the Wii is running build `0.1.60` or newer and IP/UDP is on.
   - `Live drag` sends the selected section over UDP when you release the mouse after dragging.

Command-line upload is also available:

```bat
tools\upload_layout_udp.bat 192.168.0.13 MeshLayout.config
```
