import socket
from datetime import datetime
from pathlib import Path

PORT = 44015
out_dir = Path(__file__).resolve().parent
log_path = out_dir / "udp-debug.log"


def local_ipv4_addresses():
    addresses = set()
    hostname = socket.gethostname()
    try:
        for info in socket.getaddrinfo(hostname, None, socket.AF_INET):
            address = info[4][0]
            if not address.startswith("127."):
                addresses.add(address)
    except socket.gaierror:
        pass
    try:
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        probe.connect(("8.8.8.8", 80))
        addresses.add(probe.getsockname()[0])
        probe.close()
    except OSError:
        pass
    return sorted(addresses)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(("", PORT))

print(f"Listening for WiiMesh UDP logs on port {PORT}")
print(f"Writing to {log_path}")
print("Set outputs\\debug-target.txt to one of these PC IPs, then deploy:")
for address in local_ipv4_addresses():
    print(f"  {address}")
print("Press Ctrl+C to stop.")

with log_path.open("a", encoding="utf-8", errors="replace") as f:
    f.write(f"\n--- listener started {datetime.now().isoformat(timespec='seconds')} ---\n")
    f.flush()
    while True:
        data, addr = sock.recvfrom(2048)
        text = data.decode("utf-8", errors="replace").rstrip()
        line = f"{datetime.now().isoformat(timespec='seconds')} {addr[0]}:{addr[1]} {text}"
        print(line)
        f.write(line + "\n")
        f.flush()
