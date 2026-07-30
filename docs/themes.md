# WiiMesh Themes

WiiMesh loads an optional full-screen GUI skin from:

```text
SD:/apps/wii-mesh/theme/background.rgb565
```

The image is a raw 640x480 little-endian RGB565 file. This keeps the Wii app
small and avoids requiring PNG/JPEG libraries in the homebrew build.

## Convert An Image

On Windows:

```bat
tools\make_theme_background.bat my-theme.png background.rgb565
```

If Python says Pillow is missing:

```bat
py -m pip install pillow
```

Then copy:

```text
background.rgb565 -> SD:/apps/wii-mesh/theme/background.rgb565
```

Restart WiiMesh. If the file is missing or has the wrong size, WiiMesh uses its
built-in dark Meshtastic-style dashboard skin.

## Design Notes

- Use 640x480 artwork.
- Keep important art away from the screen edges for television overscan.
- Leave the middle-left and center clear enough for readable message text.
- Dark themes work best with the current white console overlay.
