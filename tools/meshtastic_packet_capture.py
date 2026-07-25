import json
import sys
import time

from pubsub import pub
from meshtastic.serial_interface import SerialInterface


port = sys.argv[1] if len(sys.argv) > 1 else None
seconds = int(sys.argv[2]) if len(sys.argv) > 2 else 120
log_path = sys.argv[3] if len(sys.argv) > 3 else "meshtastic-packets.jsonl"


def packet_default(value):
    if isinstance(value, bytes):
        return value.hex()
    return str(value)


def on_receive(packet, interface):
    decoded = packet.get("decoded", {})
    row = {
        "time": time.time(),
        "from": packet.get("from"),
        "fromId": packet.get("fromId"),
        "to": packet.get("to"),
        "toId": packet.get("toId"),
        "channel": packet.get("channel"),
        "decoded_portnum": decoded.get("portnum"),
        "decoded_text": decoded.get("text"),
        "packet": packet,
    }
    line = json.dumps(row, default=packet_default, sort_keys=True)
    print(line, flush=True)
    with open(log_path, "a", encoding="utf-8") as f:
        f.write(line + "\n")


interface = SerialInterface(devPath=port) if port else SerialInterface()
print(f"connected {interface.devPath}; capture {seconds}s -> {log_path}", flush=True)
pub.subscribe(on_receive, "meshtastic.receive")

try:
    time.sleep(seconds)
finally:
    interface.close()
