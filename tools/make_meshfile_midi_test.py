import base64
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "outputs"
CHUNK_SIZE = 140


def vlq(value: int) -> bytes:
    parts = [value & 0x7F]
    value >>= 7
    while value:
        parts.append(0x80 | (value & 0x7F))
        value >>= 7
    return bytes(reversed(parts))


def make_midi() -> bytes:
    # Type-0, one track, 480 ticks/quarter. Short C-E-G-C test tone.
    events = bytearray()
    events += vlq(0) + bytes([0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20])
    for note in [60, 64, 67, 72]:
        events += vlq(0) + bytes([0x90, note, 96])
        events += vlq(240) + bytes([0x80, note, 0])
    events += vlq(0) + bytes([0xFF, 0x2F, 0x00])
    return (
        b"MThd"
        + (6).to_bytes(4, "big")
        + (0).to_bytes(2, "big")
        + (1).to_bytes(2, "big")
        + (480).to_bytes(2, "big")
        + b"MTrk"
        + len(events).to_bytes(4, "big")
        + bytes(events)
    )


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    midi = make_midi()
    midi_path = OUT / "wiimesh_test.mid"
    midi_path.write_bytes(midi)

    encoded = base64.b64encode(midi).decode("ascii")
    chunks = [encoded[i : i + CHUNK_SIZE] for i in range(0, len(encoded), CHUNK_SIZE)]
    lines = ["[START] wiimesh_test.mid b64"]
    for index, chunk in enumerate(chunks, start=1):
        lines.append(f"[CHUNK] {index}/{len(chunks)} wiimesh_test.mid b64:{chunk}")
    lines.append("[END] wiimesh_test.mid")

    message_path = OUT / "meshfile_midi_direct_messages.txt"
    message_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote MIDI: {midi_path}")
    print(f"Wrote direct-chat messages: {message_path}")
    print()
    print("Send these lines as DIRECT messages to the Wii node, in order:")
    for line in lines:
        print(line)


if __name__ == "__main__":
    main()
