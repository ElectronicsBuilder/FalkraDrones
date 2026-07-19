#!/usr/bin/env python3
"""Capture and summarize ToF proximity performance logs."""

from __future__ import annotations

import argparse
import json
import re
import statistics
import sys
import time
from pathlib import Path

import serial

try:
    from falkra_serial import ANSI, BAUD, PORT
except ImportError:
    ANSI = re.compile(r"\x1b\[[0-9;]*m")
    PORT = "COM14"
    BAUD = 3000000


PERF_RE = re.compile(
    r"\[TOF_PERF\]\s+"
    r"s=(?P<sensor>\d+)\s+"
    r"fps=(?P<fps>\d+(?:\.\d+)?)\s+"
    r"irq2read_us=(?P<irq2read_us>\d+)\s+"
    r"read_us=(?P<read_us>\d+)\s+"
    r"irq2snap_us=(?P<irq2snap_us>\d+)\s+"
    r"drops=(?P<drops>\d+)"
)


def percentile(values: list[float], pct: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = int(round((pct / 100.0) * (len(ordered) - 1)))
    return ordered[index]


def mean(values: list[float]) -> float:
    return statistics.fmean(values) if values else 0.0


def parse_perf_line(line: str) -> dict | None:
    match = PERF_RE.search(line)
    if not match:
        return None
    return {
        "sensor": int(match.group("sensor")),
        "fps": float(match.group("fps")),
        "irq2read_us": int(match.group("irq2read_us")),
        "read_us": int(match.group("read_us")),
        "irq2snap_us": int(match.group("irq2snap_us")),
        "drops": int(match.group("drops")),
        "line": line,
    }


def summarize(samples: list[dict]) -> dict[str, dict[str, float]]:
    by_sensor: dict[str, list[dict]] = {}
    for sample in samples:
        by_sensor.setdefault(str(sample["sensor"]), []).append(sample)

    summary: dict[str, dict[str, float]] = {}
    for sensor, sensor_samples in sorted(by_sensor.items(), key=lambda item: int(item[0])):
        fps = [float(item["fps"]) for item in sensor_samples]
        irq2snap = [float(item["irq2snap_us"]) for item in sensor_samples]
        read_us = [float(item["read_us"]) for item in sensor_samples]
        drops = [float(item["drops"]) for item in sensor_samples]
        summary[sensor] = {
            "samples": len(sensor_samples),
            "fps_mean": mean(fps),
            "irq2snap_p50_us": percentile(irq2snap, 50),
            "irq2snap_p95_us": percentile(irq2snap, 95),
            "read_us_mean": mean(read_us),
            "drops_per_sec": mean(drops),
        }
    return summary


def parse_text(text: str) -> list[dict]:
    samples = []
    for line in text.splitlines():
        sample = parse_perf_line(line)
        if sample:
            samples.append(sample)
    return samples


def print_live_header(port: str, baud: int, seconds: float) -> None:
    print(f"opening {port} @ {baud} baud")
    print(f"capturing for {seconds:g}s; Ctrl+C stops early and prints what was captured")
    print("live:  elapsed  sensor  fps  irq2read_us  read_us  irq2snap_us  drops")
    print("       -------  ------  ---  -----------  -------  -----------  -----")
    sys.stdout.flush()


def print_live_sample(elapsed: float, sample: dict) -> None:
    print(
        f"live:  {elapsed:7.1f}  "
        f"{sample['sensor']:>6}  "
        f"{sample['fps']:>3.0f}  "
        f"{sample['irq2read_us']:>11}  "
        f"{sample['read_us']:>7}  "
        f"{sample['irq2snap_us']:>11}  "
        f"{sample['drops']:>5}"
    )
    sys.stdout.flush()


def print_live_status(elapsed: float, byte_count: int, samples: list[dict]) -> None:
    sensors = sorted({sample["sensor"] for sample in samples})
    sensor_text = ",".join(str(sensor) for sensor in sensors) if sensors else "-"
    print(
        f"status: {elapsed:5.1f}s  bytes={byte_count}  "
        f"perf_lines={len(samples)}  sensors={sensor_text}"
    )
    sys.stdout.flush()


def capture(
    port: str,
    baud: int,
    seconds: float,
    *,
    live: bool,
    show_raw: bool,
    status_interval: float,
    command: str | None,
) -> tuple[str, list[dict]]:
    data = bytearray()
    samples: list[dict] = []
    line_buffer = ""
    last_status = 0.0

    if live:
        print_live_header(port, baud, seconds)

    with serial.Serial(port, baud, timeout=0.5) as ser:
        ser.reset_input_buffer()
        if command:
            ser.write(command.encode("utf-8") + b"\r\n")
            ser.flush()
            if live:
                print(f">>> sent: {command}")
                sys.stdout.flush()
        start = time.time()
        end = time.time() + seconds
        try:
            while time.time() < end:
                chunk = ser.read(8192)
                now = time.time()
                elapsed = now - start

                if chunk:
                    data.extend(chunk)
                    text = ANSI.sub("", chunk.decode("utf-8", errors="replace"))
                    line_buffer += text
                    while "\n" in line_buffer:
                        line, line_buffer = line_buffer.split("\n", 1)
                        line = line.rstrip("\r")
                        sample = parse_perf_line(line)
                        if sample:
                            samples.append(sample)
                            if live:
                                print_live_sample(elapsed, sample)
                        elif show_raw and line:
                            print(f"raw: {line}")
                            sys.stdout.flush()

                if live and status_interval > 0 and (elapsed - last_status) >= status_interval:
                    print_live_status(elapsed, len(data), samples)
                    last_status = elapsed
        except KeyboardInterrupt:
            if live:
                print("\ninterrupted; summarizing captured data")

    if line_buffer:
        line = line_buffer.rstrip("\r\n")
        sample = parse_perf_line(line)
        if sample:
            samples.append(sample)
        elif show_raw and line:
            print(f"raw: {line}")

    text = ANSI.sub("", data.decode("utf-8", errors="replace"))
    return text, samples


def save_json(path: Path, port: str, baud: int, seconds: float, samples: list[dict]) -> None:
    payload = {
        "captured_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "port": port,
        "baud": baud,
        "seconds": seconds,
        "samples": samples,
        "summary": summarize(samples),
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def load_summary(path: Path) -> dict[str, dict[str, float]]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if "summary" in payload:
        return payload["summary"]
    return summarize(payload.get("samples", []))


def print_table(summary: dict[str, dict[str, float]]) -> None:
    print("sensor  samples  fps_mean  irq2snap_p50_us  irq2snap_p95_us  read_us_mean  drops/s")
    print("------  -------  --------  ---------------  ---------------  ------------  -------")
    for sensor, item in sorted(summary.items(), key=lambda kv: int(kv[0])):
        print(
            f"{sensor:>6}  "
            f"{int(item['samples']):>7}  "
            f"{item['fps_mean']:>8.1f}  "
            f"{item['irq2snap_p50_us']:>15.0f}  "
            f"{item['irq2snap_p95_us']:>15.0f}  "
            f"{item['read_us_mean']:>12.0f}  "
            f"{item['drops_per_sec']:>7.1f}"
        )


def print_compare(current: dict[str, dict[str, float]], baseline: dict[str, dict[str, float]]) -> None:
    print("sensor  fps base->now  irq2snap_p95 base->now  read_us base->now  drops/s base->now")
    print("------  -------------  ---------------------  -----------------  -----------------")
    sensors = sorted(set(current) | set(baseline), key=int)
    for sensor in sensors:
        base = baseline.get(sensor, {})
        now = current.get(sensor, {})
        print(
            f"{sensor:>6}  "
            f"{base.get('fps_mean', 0.0):>5.1f}->{now.get('fps_mean', 0.0):<5.1f}  "
            f"{base.get('irq2snap_p95_us', 0.0):>8.0f}->{now.get('irq2snap_p95_us', 0.0):<8.0f}  "
            f"{base.get('read_us_mean', 0.0):>7.0f}->{now.get('read_us_mean', 0.0):<7.0f}  "
            f"{base.get('drops_per_sec', 0.0):>7.1f}->{now.get('drops_per_sec', 0.0):<7.1f}"
        )


def print_diagnostics(samples: list[dict]) -> None:
    if not samples:
        return

    sensors = sorted({sample["sensor"] for sample in samples})
    print(f"diagnostic: parsed {len(samples)} perf lines from sensors {','.join(map(str, sensors))}")

    if all(sample["read_us"] == 0 for sample in samples):
        print("diagnostic: all read_us values are 0; check that the firmware read timing hook is in the flashed build and DWT CYCCNT is running")

    if all(sample["irq2snap_us"] == 0 for sample in samples):
        print("diagnostic: all irq2snap_us values are 0; check the snapshot/report hook and DWT CYCCNT")

    if len(sensors) < 6:
        print(f"diagnostic: only {len(sensors)} sensor(s) reported; absent sensors will not appear in the final table")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=float, default=30.0)
    parser.add_argument("--out", type=Path, default=None)
    parser.add_argument("--compare", type=Path, default=None)
    parser.add_argument("--port", default=PORT)
    parser.add_argument("--baud", type=int, default=BAUD)
    parser.add_argument("--quiet", action="store_true", help="suppress live progress output")
    parser.add_argument("--show-raw", action="store_true", help="print non-[TOF_PERF] serial lines while capturing")
    parser.add_argument("--status-interval", type=float, default=1.0, help="seconds between live status lines")
    parser.add_argument("--cmd", default=None, help="serial command to send before capture starts")
    parser.add_argument("--restart", action="store_true", help="send the serial restart command before capture starts")
    args = parser.parse_args()
    command = "restart" if args.restart else args.cmd

    text, samples = capture(
        args.port,
        args.baud,
        args.seconds,
        live=not args.quiet,
        show_raw=args.show_raw,
        status_interval=args.status_interval,
        command=command,
    )
    if not samples:
        samples = parse_text(text)
    summary = summarize(samples)

    if args.out:
        save_json(args.out, args.port, args.baud, args.seconds, samples)
        print(f"saved {len(samples)} samples -> {args.out}")

    if not args.quiet:
        print()
        print_diagnostics(samples)
    print_table(summary)

    if args.compare:
        print()
        print_compare(summary, load_summary(args.compare))

    if not samples:
        print("no [TOF_PERF] samples captured; confirm TOF_PERF_MONITOR=1 and firmware is running", file=sys.stderr)
        return 2

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
