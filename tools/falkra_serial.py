"""Serial helper for the FFS fix campaign (COM14 @ 3M baud).

Usage:
  python falkra_serial.py capture <seconds> [outfile]
  python falkra_serial.py cmd "<command>" <seconds> [outfile]
  python falkra_serial.py watch "<until-regex>" <max_seconds> [outfile] [cmd]

watch: capture until a line matching the regex appears (then stop after a
0.5s grace period) or until max_seconds. Optionally send a command first.
Exits 0 if the marker was seen, 2 on timeout.
"""
import serial
import sys
import time
import re
from pathlib import Path

PORT = "COM14"
BAUD = 3000000

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

ANSI = re.compile(r"\x1b\[[0-9;]*m")


def run(cmd_to_send, seconds, outfile=None, until=None):
    s = serial.Serial(PORT, BAUD, timeout=0.5)
    if cmd_to_send:
        s.reset_input_buffer()
        s.write(cmd_to_send.encode() + b"\r\n")
        s.flush()
        print(f">>> sent: {cmd_to_send}")
    end = time.time() + seconds
    data = b""
    pattern = re.compile(until) if until else None
    matched = False
    while time.time() < end:
        data += s.read(8192)
        if pattern and pattern.search(ANSI.sub("", data.decode("utf-8", errors="replace"))):
            matched = True
            time.sleep(0.5)
            data += s.read(8192)
            break
    s.close()
    text = ANSI.sub("", data.decode("utf-8", errors="replace"))
    if outfile:
        outfile_path = Path(outfile)
        outfile_path.parent.mkdir(parents=True, exist_ok=True)
        with outfile_path.open("w", encoding="utf-8") as f:
            f.write(text)
        print(f"--- {len(data)} bytes -> {outfile} ---")
    print(text)
    if pattern:
        sys.exit(0 if matched else 2)


if __name__ == "__main__":
    mode = sys.argv[1]
    if mode == "capture":
        run(None, float(sys.argv[2]), sys.argv[3] if len(sys.argv) > 3 else None)
    elif mode == "cmd":
        run(sys.argv[2], float(sys.argv[3]), sys.argv[4] if len(sys.argv) > 4 else None)
    elif mode == "watch":
        run(sys.argv[5] if len(sys.argv) > 5 else None, float(sys.argv[3]),
            sys.argv[4] if len(sys.argv) > 4 else None, until=sys.argv[2])
