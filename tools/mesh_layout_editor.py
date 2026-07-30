#!/usr/bin/env python3
"""WiiMesh visual layout editor.

Creates a flat config file named MeshLayout.config. The Wii app can later load
this file from SD:/apps/wii-mesh/theme/MeshLayout.config.
"""

import argparse
import os
import socket
import tkinter as tk
from tkinter import filedialog, messagebox, ttk


CANVAS_W = 640
CANVAS_H = 480
DEFAULT_CONFIG = "MeshLayout.config"
CONSOLE_X = 28
CONSOLE_Y = 24
CELL_W = 8
CELL_H = 16


DEFAULT_SECTIONS = {
    "topbar": {
        "label": "WiiMesh",
        "x": 170,
        "y": 18,
        "w": 438,
        "h": 38,
        "text_size": 12,
        "z": 10,
        "text": "WiiMesh | version | device | Me",
    },
    "ticker": {
        "label": "Access Ticker",
        "x": 170,
        "y": 56,
        "w": 438,
        "h": 22,
        "text_size": 10,
        "z": 20,
        "text": "ACCESS latest message ticker",
    },
    "sidebar": {
        "label": "Dashboard Nav",
        "x": 18,
        "y": 74,
        "w": 132,
        "h": 264,
        "text_size": 10,
        "z": 30,
        "text": "Dashboard|Messages|Nodes|Map|Telemetry|Radio Settings",
    },
    "radio_status": {
        "label": "Radio Status",
        "x": 18,
        "y": 392,
        "w": 132,
        "h": 46,
        "text_size": 9,
        "z": 40,
        "text": "Radio Status Healthy",
    },
    "messages": {
        "label": "Messages",
        "x": 170,
        "y": 84,
        "w": 132,
        "h": 132,
        "text_size": 9,
        "z": 50,
        "text": "Messages",
    },
    "nodes": {
        "label": "Nodes",
        "x": 314,
        "y": 84,
        "w": 150,
        "h": 132,
        "text_size": 9,
        "z": 60,
        "text": "Nodes",
    },
    "map": {
        "label": "Map",
        "x": 170,
        "y": 226,
        "w": 294,
        "h": 174,
        "text_size": 10,
        "z": 70,
        "text": "Map",
    },
    "chat": {
        "label": "Chat",
        "x": 476,
        "y": 84,
        "w": 132,
        "h": 316,
        "text_size": 9,
        "z": 80,
        "text": "Chat",
    },
    "telemetry": {
        "label": "Telemetry",
        "x": 170,
        "y": 410,
        "w": 438,
        "h": 42,
        "text_size": 9,
        "z": 90,
        "text": "Telemetry",
    },
}

DEFAULT_SECTIONS.update({
    "messages_focused": {
        "label": "Messages",
        "x": 170,
        "y": 84,
        "w": 438,
        "h": 316,
        "text_size": 10,
        "z": 150,
        "text": "Focused Messages",
    },
    "nodes_focused": {
        "label": "Nodes",
        "x": 170,
        "y": 84,
        "w": 438,
        "h": 316,
        "text_size": 10,
        "z": 150,
        "text": "Focused Nodes",
    },
    "map_focused": {
        "label": "Map",
        "x": 170,
        "y": 84,
        "w": 438,
        "h": 316,
        "text_size": 10,
        "z": 150,
        "text": "Focused Map",
    },
    "telemetry_focused": {
        "label": "Telemetry",
        "x": 170,
        "y": 84,
        "w": 438,
        "h": 316,
        "text_size": 10,
        "z": 150,
        "text": "Focused Telemetry",
    },
    "chat_focused": {
        "label": "Chat",
        "x": 170,
        "y": 84,
        "w": 438,
        "h": 316,
        "text_size": 10,
        "z": 150,
        "text": "Focused Chat",
    },
    "radio_settings_focused": {
        "label": "Radio Settings",
        "x": 170,
        "y": 84,
        "w": 438,
        "h": 316,
        "text_size": 10,
        "z": 150,
        "text": "USB and stream tools live here.",
    },
})

SAMPLE_TEXT = {
    "topbar": "WiiMesh   v0.1.57     ONLINE     IP/UDP ON\nDevice: pumpkinPulse     Me: !cb05014c",
    "ticker": "ACCESS  MacnCheeseNoodles: Alert Bell Character     ACCESS",
    "sidebar": "> [H] Dashboard\n  [M] Messages\n  [N] Nodes\n  [P] Map\n  [T] Telemetry\n  [R] Radio Settings",
    "radio_status": "Radio Status\nHealthy",
    "messages": "Messages\nDM MacnCheeseNoodles  22:15\nAlert Bell Character\n29 saved",
    "nodes": "Nodes\n80 known\n* pumpkinPulse\n  Masheli 8684\n  Meshtastic 9b6c",
    "map": "Map\nMap view placeholder.\nNodes will appear here.",
    "chat": "Chat\nMacnCheeseNoodles\n\nDirect message\nAlert Bell Character\n\nRead-only client",
    "telemetry": "Telemetry\nRX 15525   TX 12   Nodes 80   Text 29\nBattery --  Voltage --  RSSI/SNR --",
    "messages_focused": "Messages\nDM MacnCheeseNoodles  22:15\nAlert Bell Character\n[OK]\n29 saved",
    "nodes_focused": "Nodes\n80 known\n* pumpkinPulse\n  Masheli 8684\n  Meshtastic 9b6c\n  OkleyNoder",
    "map_focused": "Map\nLarge map view placeholder.\nNodes and links will appear here.",
    "telemetry_focused": "Telemetry\nRX 15525   TX 12   Nodes 80   Text 29\nBattery --  Voltage --\nRSSI/SNR --",
    "chat_focused": "Chat\nMacnCheeseNoodles\n\nRead-only conversation view\nMessages with selected node appear here.",
    "radio_settings_focused": "Radio Settings\nUSB and stream tools live here.\nDebug: Stream\nWaiting for packets.",
}

COLORS = {
    "topbar": "#1daeea",
    "ticker": "#64e68b",
    "sidebar": "#4fb8ff",
    "radio_status": "#5be280",
    "messages": "#52c8ff",
    "nodes": "#b56cff",
    "map": "#f4bf25",
    "chat": "#39d0ee",
    "telemetry": "#a8d676",
    "messages_focused": "#52c8ff",
    "nodes_focused": "#b56cff",
    "map_focused": "#f4bf25",
    "telemetry_focused": "#a8d676",
    "chat_focused": "#39d0ee",
    "radio_settings_focused": "#39d0ee",
}

FILLS = {
    "topbar": "#08253a",
    "ticker": "#092838",
    "sidebar": "#031522",
    "radio_status": "#08251d",
    "messages": "#061f32",
    "nodes": "#171b38",
    "map": "#28220b",
    "chat": "#061f32",
    "telemetry": "#142419",
    "messages_focused": "#061f32",
    "nodes_focused": "#171b38",
    "map_focused": "#28220b",
    "telemetry_focused": "#142419",
    "chat_focused": "#061f32",
    "radio_settings_focused": "#061f32",
}

PREVIEW_WINDOWS = ("Dashboard", "Messages", "Nodes", "Map", "Telemetry", "Reply Chat", "Radio Settings")

FOCUSED_SECTIONS = {
    "Dashboard": {"topbar", "ticker", "sidebar", "radio_status", "messages", "nodes", "map", "chat", "telemetry"},
    "Messages": {"topbar", "ticker", "sidebar", "radio_status", "messages_focused"},
    "Nodes": {"topbar", "ticker", "sidebar", "radio_status", "nodes_focused"},
    "Map": {"topbar", "ticker", "sidebar", "radio_status", "map_focused"},
    "Telemetry": {"topbar", "ticker", "sidebar", "radio_status", "telemetry_focused"},
    "Reply Chat": {"topbar", "ticker", "sidebar", "radio_status", "chat_focused"},
    "Radio Settings": {"topbar", "ticker", "sidebar", "radio_status", "radio_settings_focused"},
}


def clamp(value, low, high):
    return max(low, min(high, value))


def parse_value(value):
    value = value.strip()
    try:
        return int(value)
    except ValueError:
        return value


def load_config(path):
    data = {name: dict(values) for name, values in DEFAULT_SECTIONS.items()}
    if not path or not os.path.exists(path):
        return data
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, value = line.split("=", 1)
            parts = key.strip().split(".", 1)
            if len(parts) != 2:
                continue
            section, field = parts
            if section == "canvas":
                continue
            if section not in data:
                data[section] = {"label": section, "x": 0, "y": 0, "w": 80, "h": 40, "text_size": 10, "z": 100, "text": section}
            data[section][field] = parse_value(value)
    return data


def save_config(path, data):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("# WiiMesh GUI layout\n")
        f.write("# Copy to SD:/apps/wii-mesh/theme/MeshLayout.config\n")
        f.write("canvas.w=640\n")
        f.write("canvas.h=480\n")
        for name in data:
            section = data[name]
            f.write("\n")
            for field in ("label", "x", "y", "w", "h", "text_size", "z", "text"):
                value = str(section.get(field, ""))
                value = value.replace("\n", " ").replace("\r", " ")
                f.write(f"{name}.{field}={value}\n")


def config_lines(data):
    lines = []
    for name in data:
        section = data[name]
        for field in ("label", "x", "y", "w", "h", "text_size", "z", "text"):
            value = str(section.get(field, ""))
            value = value.replace("\n", " ").replace("\r", " ")
            lines.append(f"{name}.{field}={value}")
    return lines


def send_udp(host, command, timeout=4.0, attempts=3):
    payload = (command.strip() + "\n").encode("utf-8")
    last_error = None
    for _attempt in range(attempts):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(timeout)
        try:
            sock.sendto(payload, (host, 44016))
            data, _addr = sock.recvfrom(65535)
            return data.decode("utf-8", "replace")
        except OSError as e:
            last_error = e
        finally:
            sock.close()
    raise last_error or TimeoutError("No UDP reply")


class LayoutEditor:
    def __init__(self, root, path):
        self.root = root
        self.path = os.path.abspath(path or DEFAULT_CONFIG)
        self.data = load_config(self.path)
        self.selected = "sidebar"
        self.items = {}
        self.handles = {}
        self.drag = None
        self.live_drag_var = tk.BooleanVar(value=False)
        self.snap_grid_var = tk.BooleanVar(value=True)
        self.preview_window_var = tk.StringVar(value="Dashboard")
        self.preview_focus_var = tk.BooleanVar(value=False)
        self.transition_var = tk.StringVar(value="None")

        root.title("WiiMesh MeshLayout.config Editor")
        root.geometry("980x620")

        outer = ttk.Frame(root, padding=10)
        outer.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(outer)
        left.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.canvas = tk.Canvas(left, width=CANVAS_W, height=CANVAS_H, bg="#06131f", highlightthickness=0)
        self.canvas.pack()
        self.canvas.bind("<ButtonPress-1>", self.on_press)
        self.canvas.bind("<B1-Motion>", self.on_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_release)

        hint = ttk.Label(
            left,
            text="Drag inside a box to move. Drag a corner square to resize. Shift-drag also resizes. Ctrl+S saves.",
        )
        hint.pack(anchor="w", pady=(8, 0))

        right = ttk.Frame(outer, width=280)
        right.pack(side=tk.RIGHT, fill=tk.Y, padx=(12, 0))

        ttk.Label(right, text="Section").pack(anchor="w")
        self.section_var = tk.StringVar(value=self.selected)
        self.section_combo = ttk.Combobox(
            right,
            textvariable=self.section_var,
            values=list(self.data.keys()),
            state="readonly",
        )
        self.section_combo.pack(fill=tk.X, pady=(0, 10))
        self.section_combo.bind("<<ComboboxSelected>>", self.on_section_combo)

        ttk.Label(right, text="Preview window").pack(anchor="w")
        self.preview_combo = ttk.Combobox(
            right,
            textvariable=self.preview_window_var,
            values=PREVIEW_WINDOWS,
            state="readonly",
        )
        self.preview_combo.pack(fill=tk.X, pady=(0, 6))
        self.preview_combo.bind("<<ComboboxSelected>>", lambda _e: self.redraw())
        ttk.Checkbutton(right, text="Focused preview", variable=self.preview_focus_var, command=self.redraw).pack(anchor="w", pady=(0, 10))
        ttk.Label(right, text="Transition preview").pack(anchor="w")
        self.transition_combo = ttk.Combobox(
            right,
            textvariable=self.transition_var,
            values=("None", "Enter from right", "Enter from left"),
            state="readonly",
        )
        self.transition_combo.pack(fill=tk.X, pady=(0, 10))
        self.transition_combo.bind("<<ComboboxSelected>>", lambda _e: self.redraw())

        self.vars = {}
        for field in ("label", "x", "y", "w", "h", "text_size", "z"):
            ttk.Label(right, text=field).pack(anchor="w")
            var = tk.StringVar()
            entry = ttk.Entry(right, textvariable=var)
            entry.pack(fill=tk.X, pady=(0, 8))
            entry.bind("<Return>", self.apply_fields)
            entry.bind("<FocusOut>", self.apply_fields)
            self.vars[field] = var

        ttk.Label(right, text="text").pack(anchor="w")
        self.text_box = tk.Text(right, width=28, height=5, wrap=tk.WORD)
        self.text_box.pack(fill=tk.X, pady=(0, 8))
        self.text_box.bind("<FocusOut>", self.apply_fields)
        self.text_box.bind("<Control-Return>", self.apply_fields)

        buttons = ttk.Frame(right)
        buttons.pack(fill=tk.X, pady=(8, 0))
        ttk.Button(buttons, text="Apply", command=self.apply_fields).pack(side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Button(buttons, text="Reset", command=self.reset_selected).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(6, 0))

        ttk.Separator(right).pack(fill=tk.X, pady=12)
        ttk.Label(right, text="Wii UDP IP").pack(anchor="w")
        self.ip_var = tk.StringVar(value="192.168.0.13")
        ttk.Entry(right, textvariable=self.ip_var).pack(fill=tk.X, pady=(0, 8))
        udp_buttons = ttk.Frame(right)
        udp_buttons.pack(fill=tk.X)
        ttk.Button(udp_buttons, text="Upload Live", command=self.udp_upload).pack(side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Button(udp_buttons, text="Download", command=self.udp_download).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(6, 0))
        udp_buttons2 = ttk.Frame(right)
        udp_buttons2.pack(fill=tk.X, pady=(6, 0))
        ttk.Button(udp_buttons2, text="Import Data", command=self.udp_import_live_data).pack(side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Button(udp_buttons2, text="Reload Wii", command=self.udp_reload).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(6, 0))
        udp_buttons3 = ttk.Frame(right)
        udp_buttons3.pack(fill=tk.X, pady=(6, 0))
        ttk.Button(udp_buttons3, text="Test Move Map", command=self.udp_test_move).pack(side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Checkbutton(udp_buttons3, text="Live drag", variable=self.live_drag_var).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Checkbutton(right, text="Snap text grid", variable=self.snap_grid_var, command=self.snap_selected).pack(anchor="w", pady=(6, 0))

        ttk.Separator(right).pack(fill=tk.X, pady=12)
        ttk.Button(right, text="Open Config", command=self.open_config).pack(fill=tk.X)
        ttk.Button(right, text="Save", command=self.save).pack(fill=tk.X, pady=(6, 0))
        ttk.Button(right, text="Save As", command=self.save_as).pack(fill=tk.X, pady=(6, 0))
        ttk.Button(right, text="Reset All", command=self.reset_all).pack(fill=tk.X, pady=(6, 0))

        self.status = tk.StringVar(value=f"Editing {self.path}")
        ttk.Label(right, textvariable=self.status, wraplength=260).pack(anchor="w", pady=(12, 0))

        root.bind("<Up>", lambda e: self.nudge(0, -1))
        root.bind("<Down>", lambda e: self.nudge(0, 1))
        root.bind("<Left>", lambda e: self.nudge(-1, 0))
        root.bind("<Right>", lambda e: self.nudge(1, 0))
        root.bind("<Shift-Up>", lambda e: self.resize(0, -1))
        root.bind("<Shift-Down>", lambda e: self.resize(0, 1))
        root.bind("<Shift-Left>", lambda e: self.resize(-1, 0))
        root.bind("<Shift-Right>", lambda e: self.resize(1, 0))
        root.bind("<Control-s>", lambda e: self.save())

        self.redraw()
        self.load_fields()

    def redraw(self):
        self.canvas.delete("all")
        self.draw_background()
        self.items.clear()
        self.handles.clear()
        visible = set(self.data.keys())
        if self.preview_focus_var.get():
            visible = FOCUSED_SECTIONS.get(self.preview_window_var.get(), visible)
        for name, section in sorted(self.data.items(), key=lambda item: int(item[1].get("z", 0))):
            if name not in visible:
                continue
            x = int(section.get("x", 0))
            y = int(section.get("y", 0))
            if self.transition_var.get() == "Enter from right" and name not in ("topbar", "ticker", "sidebar", "radio_status"):
                x += 48
            elif self.transition_var.get() == "Enter from left" and name not in ("topbar", "ticker", "sidebar", "radio_status"):
                x -= 48
            w = int(section.get("w", 80))
            h = int(section.get("h", 40))
            color = COLORS.get(name, "#ffffff")
            fill = FILLS.get(name, "#0a2235")
            width = 3 if name == self.selected else 1
            rect = self.canvas.create_rectangle(x, y, x + w, y + h, outline=color, width=width, fill=fill)
            title = str(section.get("label", name))
            has_title = bool(title.strip())
            sample = str(section.get("text", "")).strip() or SAMPLE_TEXT.get(name, title)
            default_text = str(DEFAULT_SECTIONS.get(name, {}).get("text", ""))
            if sample == title or sample == default_text:
                sample = SAMPLE_TEXT.get(name, sample)
            try:
                text_size = clamp(int(section.get("text_size", 9)), 6, 24)
            except (TypeError, ValueError):
                text_size = 9
            title_item = None
            if has_title:
                title_item = self.canvas.create_text(
                    x + 6,
                    y + 5,
                    anchor="nw",
                    fill="#e8f2ff",
                    text=title,
                    font=("Consolas", 10, "bold"),
                    width=max(20, w - 12),
                )
            sample_item = self.canvas.create_text(
                x + 6,
                y + (22 if has_title else 5),
                anchor="nw",
                fill="#d2e3ef",
                text=sample,
                font=("Consolas", text_size),
                width=max(20, w - 12),
            )
            self.items[rect] = name
            if title_item is not None:
                self.items[title_item] = name
            self.items[sample_item] = name
            if name == self.selected:
                self.draw_handles(name, x, y, w, h, color)

    def draw_handles(self, name, x, y, w, h, color):
        points = {
            "nw": (x, y),
            "ne": (x + w, y),
            "sw": (x, y + h),
            "se": (x + w, y + h),
        }
        for handle, (hx, hy) in points.items():
            item = self.canvas.create_rectangle(
                hx - 5,
                hy - 5,
                hx + 5,
                hy + 5,
                outline="#ffffff",
                fill=color,
                width=1,
            )
            self.items[item] = name
            self.handles[item] = handle

    def draw_background(self):
        self.canvas.create_rectangle(0, 0, CANVAS_W, CANVAS_H, fill="#06131f", outline="")
        self.canvas.create_rectangle(8, 8, 632, 472, outline="#18526f", fill="#061b2d")
        self.canvas.create_rectangle(10, 52, 160, 462, outline="#18526f", fill="#04101c")
        self.canvas.create_rectangle(10, 10, 632, 52, outline="#18526f", fill="#071e32")
        for x in range(180, 540, 36):
            self.canvas.create_line(x, 86, x - 120, 360, fill="#0d3554")
        for y in range(80, 360, 28):
            self.canvas.create_line(170, y, 608, y - 36, fill="#0d3554")
        self.canvas.create_rectangle(
            CONSOLE_X,
            CONSOLE_Y,
            CONSOLE_X + 584,
            CONSOLE_Y + 432,
            outline="#38556a",
            dash=(2, 4),
        )
        for x in range(CONSOLE_X, CONSOLE_X + 584, CELL_W * 8):
            self.canvas.create_line(x, CONSOLE_Y, x, CONSOLE_Y + 432, fill="#10283a")
        for y in range(CONSOLE_Y, CONSOLE_Y + 432, CELL_H * 4):
            self.canvas.create_line(CONSOLE_X, y, CONSOLE_X + 584, y, fill="#10283a")

    def select(self, name):
        if name not in self.data:
            return
        self.selected = name
        self.section_var.set(name)
        self.load_fields()
        self.redraw()

    def load_fields(self):
        section = self.data[self.selected]
        for field, var in self.vars.items():
            var.set(str(section.get(field, "")))
        self.text_box.delete("1.0", tk.END)
        self.text_box.insert("1.0", str(section.get("text", "")))

    def apply_fields(self, event=None):
        section = self.data[self.selected]
        for field, var in self.vars.items():
            value = var.get()
            if field in ("x", "y", "w", "h", "text_size", "z"):
                try:
                    value = int(value)
                except ValueError:
                    value = int(section.get(field, 0))
                if field == "text_size":
                    value = clamp(value, 6, 24)
                if field == "z":
                    value = clamp(value, 0, 9999)
            section[field] = value
        section["text"] = self.text_box.get("1.0", "end-1c")
        self.constrain(self.selected)
        if self.snap_grid_var.get():
            self.snap_section(self.selected)
        self.load_fields()
        self.redraw()

    def constrain(self, name):
        section = self.data[name]
        section["w"] = clamp(int(section.get("w", 80)), 8, CANVAS_W)
        section["h"] = clamp(int(section.get("h", 40)), 8, CANVAS_H)
        section["x"] = clamp(int(section.get("x", 0)), 0, CANVAS_W - section["w"])
        section["y"] = clamp(int(section.get("y", 0)), 0, CANVAS_H - section["h"])

    def snap_value(self, value, origin, cell):
        return origin + round((int(value) - origin) / cell) * cell

    def snap_section(self, name):
        if name not in self.data:
            return
        section = self.data[name]
        section["x"] = self.snap_value(section.get("x", 0), CONSOLE_X, CELL_W)
        section["y"] = self.snap_value(section.get("y", 0), CONSOLE_Y, CELL_H)
        section["w"] = max(CELL_W, round(int(section.get("w", 80)) / CELL_W) * CELL_W)
        section["h"] = max(CELL_H, round(int(section.get("h", 40)) / CELL_H) * CELL_H)
        self.constrain(name)

    def snap_selected(self):
        if self.snap_grid_var.get():
            self.snap_section(self.selected)
            self.load_fields()
            self.redraw()

    def on_section_combo(self, event=None):
        self.select(self.section_var.get())

    def on_press(self, event):
        item = self.pick_item(event.x, event.y)
        if item is None:
            return
        name = self.items.get(item)
        if not name:
            return
        handle = self.handles.get(item)
        self.select(name)
        self.bring_to_front(name)
        section = self.data[name]
        self.drag = {
            "name": name,
            "x": event.x,
            "y": event.y,
            "start_x": int(section.get("x", 0)),
            "start_y": int(section.get("y", 0)),
            "start_w": int(section.get("w", 80)),
            "start_h": int(section.get("h", 40)),
            "mode": "resize" if handle or bool(event.state & 0x0001) else "move",
            "handle": handle or "se",
        }

    def bring_to_front(self, name):
        if name not in self.data:
            return
        top = max(int(section.get("z", 0)) for section in self.data.values())
        self.data[name]["z"] = top + 10
        self.load_fields()
        self.redraw()

    def pick_item(self, x, y):
        overlapping = self.canvas.find_overlapping(x, y, x, y)
        for item in reversed(overlapping):
            if item in self.items:
                return item
        return None

    def on_drag(self, event):
        if not self.drag:
            return
        section = self.data[self.drag["name"]]
        dx = event.x - self.drag["x"]
        dy = event.y - self.drag["y"]
        if self.drag["mode"] == "resize":
            handle = self.drag["handle"]
            if "e" in handle:
                section["w"] = self.drag["start_w"] + dx
            if "s" in handle:
                section["h"] = self.drag["start_h"] + dy
            if "w" in handle:
                section["x"] = self.drag["start_x"] + dx
                section["w"] = self.drag["start_w"] - dx
            if "n" in handle:
                section["y"] = self.drag["start_y"] + dy
                section["h"] = self.drag["start_h"] - dy
        else:
            section["x"] = self.drag["start_x"] + dx
            section["y"] = self.drag["start_y"] + dy
        self.constrain(self.drag["name"])
        if self.snap_grid_var.get():
            self.snap_section(self.drag["name"])
        self.load_fields()
        self.redraw()

    def on_release(self, event):
        if self.drag and self.live_drag_var.get():
            self.udp_upload_section(self.drag["name"], save=False)
        self.drag = None

    def nudge(self, dx, dy):
        section = self.data[self.selected]
        step = 10 if self.root.focus_get() is None else 4
        section["x"] = int(section.get("x", 0)) + dx * step
        section["y"] = int(section.get("y", 0)) + dy * step
        self.constrain(self.selected)
        if self.snap_grid_var.get():
            self.snap_section(self.selected)
        self.load_fields()
        self.redraw()

    def resize(self, dx, dy):
        section = self.data[self.selected]
        section["w"] = int(section.get("w", 80)) + dx * 4
        section["h"] = int(section.get("h", 40)) + dy * 4
        self.constrain(self.selected)
        if self.snap_grid_var.get():
            self.snap_section(self.selected)
        self.load_fields()
        self.redraw()

    def reset_selected(self):
        if self.selected in DEFAULT_SECTIONS:
            self.data[self.selected] = dict(DEFAULT_SECTIONS[self.selected])
            self.load_fields()
            self.redraw()

    def reset_all(self):
        if messagebox.askyesno("Reset All", "Reset every section to the default WiiMesh layout?"):
            self.data = {name: dict(values) for name, values in DEFAULT_SECTIONS.items()}
            self.section_combo["values"] = list(self.data.keys())
            self.select("sidebar")

    def open_config(self):
        path = filedialog.askopenfilename(
            title="Open MeshLayout.config",
            filetypes=[("WiiMesh config", "*.config"), ("All files", "*.*")],
        )
        if not path:
            return
        self.path = path
        self.data = load_config(path)
        self.section_combo["values"] = list(self.data.keys())
        self.select(next(iter(self.data)))
        self.status.set(f"Editing {self.path}")

    def save(self):
        self.apply_fields()
        save_config(self.path, self.data)
        self.status.set(f"Saved {self.path}")

    def save_as(self):
        path = filedialog.asksaveasfilename(
            title="Save MeshLayout.config",
            initialfile=DEFAULT_CONFIG,
            defaultextension=".config",
            filetypes=[("WiiMesh config", "*.config"), ("All files", "*.*")],
        )
        if not path:
            return
        self.path = path
        self.save()

    def udp_upload(self):
        self.apply_fields()
        host = self.ip_var.get().strip()
        if not host:
            messagebox.showerror("Missing IP", "Enter the Wii IP address.")
            return
        try:
            count = 0
            for line in config_lines(self.data):
                send_udp(host, "LAYOUT_SET " + line)
                count += 1
            reply = send_udp(host, "LAYOUT_SAVE")
            verify = self.verify_wii_layout(host)
            self.status.set(f"Uploaded {count} values over UDP. {reply.strip()} {verify}")
        except OSError as e:
            messagebox.showerror("UDP Upload Failed", str(e))

    def udp_upload_section(self, name, save=False):
        host = self.ip_var.get().strip()
        if not host or name not in self.data:
            return
        section = self.data[name]
        try:
            for field in ("label", "x", "y", "w", "h", "text_size", "z", "text"):
                value = str(section.get(field, "")).replace("\n", " ").replace("\r", " ")
                send_udp(host, f"LAYOUT_SET {name}.{field}={value}", timeout=2.5, attempts=2)
            if save:
                send_udp(host, "LAYOUT_SAVE", timeout=3.0, attempts=2)
            self.status.set(f"Sent {name} over UDP")
        except OSError as e:
            self.status.set(f"Live drag UDP failed: {e}")

    def verify_wii_layout(self, host):
        try:
            text = send_udp(host, "LAYOUT_GET", timeout=5.0)
        except OSError as e:
            return f"Verify failed: {e}"
        remote = self.parse_config_text(text)
        mismatches = []
        for name, section in self.data.items():
            if name not in remote:
                mismatches.append(name)
                continue
            for field in ("x", "y", "w", "h", "text_size", "z"):
                if int(section.get(field, 0)) != int(remote[name].get(field, -9999)):
                    mismatches.append(f"{name}.{field}")
                    break
        if mismatches:
            return "VERIFY MISMATCH " + ", ".join(mismatches[:4])
        return "Verified."

    def parse_config_text(self, text):
        temp_path = self.path + ".udp.tmp"
        with open(temp_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        try:
            return load_config(temp_path)
        finally:
            try:
                os.remove(temp_path)
            except OSError:
                pass

    def udp_download(self):
        host = self.ip_var.get().strip()
        if not host:
            messagebox.showerror("Missing IP", "Enter the Wii IP address.")
            return
        try:
            text = send_udp(host, "LAYOUT_GET", timeout=5.0)
        except OSError as e:
            messagebox.showerror("UDP Download Failed", str(e))
            return
        self.data = self.parse_config_text(text)
        self.section_combo["values"] = list(self.data.keys())
        self.select(next(iter(self.data)))
        save_config(self.path, self.data)
        self.status.set("Downloaded current Wii layout over UDP.")

    def udp_reload(self):
        host = self.ip_var.get().strip()
        if not host:
            messagebox.showerror("Missing IP", "Enter the Wii IP address.")
            return
        try:
            reply = send_udp(host, "LAYOUT")
            self.status.set(reply.strip())
        except OSError as e:
            messagebox.showerror("UDP Reload Failed", str(e))

    def udp_import_live_data(self):
        host = self.ip_var.get().strip()
        if not host:
            messagebox.showerror("Missing IP", "Enter the Wii IP address.")
            return
        try:
            text = send_udp(host, "LIVE_DATA", timeout=5.0)
        except OSError as e:
            messagebox.showerror("Live Data Failed", str(e))
            return
        changed = 0
        for raw in text.splitlines():
            if "=" not in raw:
                continue
            key, value = raw.split("=", 1)
            section, dot, field = key.partition(".")
            if dot and field == "text" and section in self.data:
                self.data[section]["text"] = value.replace(" | ", "\n")
                changed += 1
        self.load_fields()
        self.redraw()
        self.status.set(f"Imported live Wii preview text for {changed} sections.")

    def udp_test_move(self):
        host = self.ip_var.get().strip()
        if not host:
            messagebox.showerror("Missing IP", "Enter the Wii IP address.")
            return
        try:
            send_udp(host, "LAYOUT_SET map.x=40")
            send_udp(host, "LAYOUT_SET map.y=80")
            send_udp(host, "LAYOUT_SET map.w=240")
            send_udp(host, "LAYOUT_SET map.h=140")
            self.status.set("Sent test map move. Wii map panel should jump left.")
        except OSError as e:
            messagebox.showerror("UDP Test Move Failed", str(e))


def main():
    parser = argparse.ArgumentParser(description="Edit WiiMesh GUI layout config.")
    parser.add_argument("config", nargs="?", default=DEFAULT_CONFIG, help="MeshLayout.config path")
    args = parser.parse_args()
    root = tk.Tk()
    LayoutEditor(root, args.config)
    root.mainloop()


if __name__ == "__main__":
    main()
