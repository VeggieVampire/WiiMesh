import argparse
import random
import re
import socket
import sys

try:
    from meshtastic.protobuf import admin_pb2, config_pb2, mesh_pb2, portnums_pb2
except Exception as exc:
    print("Meshtastic Python protobufs are required.")
    print("Install with: py -m pip install --user meshtastic")
    raise SystemExit(1) from exc


PORT = 44016
BROADCAST = 0xFFFFFFFF


def udp_command(host: str, command: str, timeout: float = 2.0) -> str:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    sock.sendto((command.strip() + "\n").encode("utf-8"), (host, PORT))
    try:
        data, _ = sock.recvfrom(65535)
        return data.decode("utf-8", "replace")
    except socket.timeout:
        return ""
    finally:
        sock.close()


def parse_node_num(text: str) -> int:
    match = re.search(r"\bnode\s+!(?P<hex>[0-9a-fA-F]{8})\b", text)
    if match:
        return int(match.group("hex"), 16)
    match = re.search(r"\bMe:\s*!(?P<hex>[0-9a-fA-F]{8})\b", text)
    if match:
        return int(match.group("hex"), 16)
    return 0


def node_arg(value: str) -> int:
    value = value.strip()
    if value.lower() in ("self", "local", "me", "auto"):
        return 0
    if value.startswith("!"):
        value = value[1:]
    if value.lower().startswith("0x"):
        value = value[2:]
    return int(value[-8:], 16)


def as_hex(data: bytes) -> str:
    return data.hex()


def build_to_radio_admin(admin: "admin_pb2.AdminMessage", dest: int, channel: int,
                         want_ack: bool = True, want_response: bool = True) -> bytes:
    packet = mesh_pb2.MeshPacket()
    packet.to = dest
    packet.channel = channel
    packet.want_ack = want_ack
    packet.id = random.randint(1, 0xFFFFFFFF)
    packet.pki_encrypted = True
    packet.decoded.portnum = portnums_pb2.PortNum.ADMIN_APP
    packet.decoded.payload = admin.SerializeToString()
    packet.decoded.want_response = want_response

    to_radio = mesh_pb2.ToRadio()
    to_radio.packet.CopyFrom(packet)
    return to_radio.SerializeToString()


def admin_get_config(config_type: int) -> "admin_pb2.AdminMessage":
    admin = admin_pb2.AdminMessage()
    admin.get_config_request = config_type
    return admin


def admin_get_owner() -> "admin_pb2.AdminMessage":
    admin = admin_pb2.AdminMessage()
    admin.get_owner_request = True
    return admin


def admin_get_metadata() -> "admin_pb2.AdminMessage":
    admin = admin_pb2.AdminMessage()
    admin.get_device_metadata_request = True
    return admin


def admin_set_network(args: argparse.Namespace) -> "admin_pb2.AdminMessage":
    admin = admin_pb2.AdminMessage()
    if args.session_passkey:
        admin.session_passkey = bytes.fromhex(args.session_passkey.replace(" ", ""))
    network = config_pb2.Config.NetworkConfig()
    if args.wifi_enabled is not None:
        network.wifi_enabled = args.wifi_enabled
    if args.ssid is not None:
        network.wifi_ssid = args.ssid
    if args.psk is not None:
        network.wifi_psk = args.psk
    if args.ntp is not None:
        network.ntp_server = args.ntp
    if args.udp_broadcast is not None:
        network.enabled_protocols = 1 if args.udp_broadcast else 0
    admin.set_config.network.CopyFrom(network)
    return admin


def send_to_radio(host: str, payload: bytes) -> str:
    return udp_command(host, "TO_RADIO_HEX " + as_hex(payload), timeout=3.0)


def main() -> int:
    parser = argparse.ArgumentParser(description="Send official Meshtastic protobuf admin/control packets through WiiMesh UDP.")
    parser.add_argument("host", nargs="?", default="192.168.0.42", help="Wii IP address")
    parser.add_argument("--to", default="auto", help="Destination node, usually auto/self/!12345678")
    parser.add_argument("--channel", type=int, default=0, help="Admin channel index, default 0")
    sub = parser.add_subparsers(dest="cmd", required=True)

    def add_route_options(p: argparse.ArgumentParser) -> None:
        p.add_argument("--to", dest="cmd_to", help="Destination node, usually auto/self/!12345678")
        p.add_argument("--channel", dest="cmd_channel", type=int, help="Admin channel index")

    sub.add_parser("status", help="Ask WiiMesh for RX status")
    sub.add_parser("text-log", help="Ask WiiMesh for decoded/stored text messages")
    for name, help_text in (
        ("request-session", "Request admin session key config"),
        ("get-network", "Request Meshtastic network config"),
        ("get-owner", "Request owner/user info over admin"),
        ("get-metadata", "Request device metadata over admin"),
    ):
        cmd_parser = sub.add_parser(name, help=help_text)
        add_route_options(cmd_parser)

    set_net = sub.add_parser("set-network", help="Send NetworkConfig. Writes usually require --session-passkey from request-session response.")
    add_route_options(set_net)
    set_net.add_argument("--session-passkey", default="", help="Hex session key from Admin SESSIONKEY_CONFIG response")
    set_net.add_argument("--wifi-enabled", action=argparse.BooleanOptionalAction, default=None)
    set_net.add_argument("--ssid")
    set_net.add_argument("--psk")
    set_net.add_argument("--ntp")
    set_net.add_argument("--udp-broadcast", action=argparse.BooleanOptionalAction, default=None)

    args = parser.parse_args()
    if args.cmd == "status":
        print(udp_command(args.host, "RX_STATUS", timeout=3.0) or "no UDP reply")
        return 0
    if args.cmd == "text-log":
        print(udp_command(args.host, "TEXT_LOG", timeout=3.0) or "no UDP reply")
        return 0

    if getattr(args, "cmd_to", None):
        args.to = args.cmd_to
    if getattr(args, "cmd_channel", None) is not None:
        args.channel = args.cmd_channel

    dest = node_arg(args.to)
    if dest == 0:
        status = udp_command(args.host, "RX_STATUS", timeout=3.0)
        dest = parse_node_num(status)
        if dest == 0:
            print("Could not detect WiiMesh node number. Use --to !12345678.")
            print(status or "RX_STATUS had no reply.")
            return 1

    if args.cmd == "request-session":
        admin = admin_get_config(admin_pb2.AdminMessage.SESSIONKEY_CONFIG)
    elif args.cmd == "get-network":
        admin = admin_get_config(admin_pb2.AdminMessage.NETWORK_CONFIG)
    elif args.cmd == "get-owner":
        admin = admin_get_owner()
    elif args.cmd == "get-metadata":
        admin = admin_get_metadata()
    elif args.cmd == "set-network":
        admin = admin_set_network(args)
    else:
        parser.error("unknown command")

    payload = build_to_radio_admin(admin, dest, args.channel)
    print(f"dest=!{dest:08x} channel={args.channel} ToRadio bytes={len(payload)}")
    print(send_to_radio(args.host, payload) or "no UDP command reply")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
