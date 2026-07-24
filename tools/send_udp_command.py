import socket
import sys

host = sys.argv[1] if len(sys.argv) > 1 else "192.168.0.13"
command = sys.argv[2] if len(sys.argv) > 2 else "PING"
port = 44016

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto((command.strip().upper() + "\n").encode("ascii"), (host, port))
print(f"sent {command.strip().upper()} to {host}:{port}")
