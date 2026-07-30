import socket
import sys


def send(host, command, timeout=4.0, attempts=3):
    payload = (command.strip() + "\n").encode("utf-8")
    last_error = None
    for _attempt in range(attempts):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(timeout)
        try:
            sock.sendto(payload, (host, 44016))
            data, _addr = sock.recvfrom(65535)
            return data.decode("utf-8", "replace").strip()
        except OSError as e:
            last_error = e
        finally:
            sock.close()
    raise last_error or TimeoutError("No UDP reply")


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else "192.168.0.13"
    path = sys.argv[2] if len(sys.argv) > 2 else "MeshLayout.config"
    count = 0
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#") or line.startswith("canvas."):
                continue
            if "=" not in line:
                continue
            print(send(host, "LAYOUT_SET " + line))
            count += 1
    print(send(host, "LAYOUT_SAVE"))
    print(f"uploaded {count} layout values to {host}")


if __name__ == "__main__":
    main()
