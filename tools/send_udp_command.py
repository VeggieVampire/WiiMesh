import socket
import sys

host = sys.argv[1] if len(sys.argv) > 1 else "192.168.0.13"
command = " ".join(sys.argv[2:]) if len(sys.argv) > 2 else "PING"
port = 44016

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
line = command.strip()
sock.sendto((line + "\n").encode("utf-8"), (host, port))
print(f"sent {line} to {host}:{port}")
sock.settimeout(2.0)
try:
    data, addr = sock.recvfrom(65535)
    print(f"reply from {addr[0]}:{addr[1]}: {data.decode('utf-8', 'replace').strip()}")
except socket.timeout:
    print("no UDP command reply")
