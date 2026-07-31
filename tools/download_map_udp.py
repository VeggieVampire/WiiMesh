import datetime
import pathlib
import socket
import sys


if len(sys.argv) > 1 and sys.argv[1] in ("-h", "--help", "/?"):
    print("usage: download_map_udp.py [wii_ip]")
    print("downloads the current WiiMesh map over UDP port 44016")
    sys.exit(0)

host = sys.argv[1] if len(sys.argv) > 1 else "192.168.0.42"
port = 44016
script_dir = pathlib.Path(__file__).resolve().parent
if script_dir.name.lower() == "outputs":
    out_dir = script_dir
else:
    out_dir = pathlib.Path(__file__).resolve().parents[1] / ".." / ".." / "outputs"
out_dir.mkdir(parents=True, exist_ok=True)
stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
out_path = out_dir / f"mesh_map_udp_{stamp}_{host}.dat"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(3.0)
sock.sendto(b"MAP_GET\n", (host, port))
print(f"sent MAP_GET to {host}:{port}")

try:
    data, addr = sock.recvfrom(65535)
except socket.timeout:
    print("no UDP map reply")
    sys.exit(1)

text = data.decode("utf-8", "replace")
out_path.write_text(text, encoding="utf-8")
print(f"reply from {addr[0]}:{addr[1]}")
print(f"saved {out_path}")
print(text.rstrip())
