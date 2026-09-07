#!/usr/bin/env python3
"""Validate the live installation scheduler from its OSC state stream."""

import argparse
import os
import socket
import struct
import subprocess
import time
from pathlib import Path


def read_string(data, offset):
    end = data.index(b"\0", offset)
    value = data[offset:end].decode("utf-8", errors="replace")
    return value, (end + 4) & ~3


def messages(packet):
    if packet.startswith(b"#bundle\0"):
        offset = 16
        while offset + 4 <= len(packet):
            size = struct.unpack_from(">i", packet, offset)[0]
            offset += 4
            yield from messages(packet[offset:offset + size])
            offset += size
        return

    try:
        address, offset = read_string(packet, 0)
        tags, offset = read_string(packet, offset)
    except (ValueError, UnicodeDecodeError):
        return
    values = []
    for tag in tags[1:]:
        if tag == "i":
            values.append(struct.unpack_from(">i", packet, offset)[0])
            offset += 4
        elif tag == "f":
            values.append(struct.unpack_from(">f", packet, offset)[0])
            offset += 4
        elif tag == "s":
            value, offset = read_string(packet, offset)
            values.append(value)
        else:
            return
    yield address, values


def run_once(app, duration, pulse_seconds, bar_seconds, port, seed):
    receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    receiver.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    receiver.bind(("127.0.0.1", port))
    receiver.settimeout(0.25)

    environment = os.environ.copy()
    environment.update({
        "PDJ_COMPOSER_ENABLED": "1",
        "PDJ_PULSE_MOMENT_SECONDS": str(pulse_seconds),
        "PDJ_BARSCAN_MOMENT_SECONDS": str(bar_seconds),
        "PDJ_TAKEOVER_INTERVAL_SECONDS": "120",
        "PDJ_OSC_PORT": str(port),
        # Una semilla distinta de cero fija la ejecución; los ajustes de distribución usan 0 para que
        # la instalación arranque de forma distinta en cada lanzamiento.
        "PDJ_COMPOSER_SEED": str(seed),
    })
    process = subprocess.Popen(
        [str(app)], env=environment, stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )
    started = time.monotonic()
    moments = []
    moment_times = []
    occupancy = [0, 0]
    content = {}
    signature = None
    maximum_occupancy = 0
    try:
        while time.monotonic() - started < duration:
            try:
                packet, _ = receiver.recvfrom(65535)
            except socket.timeout:
                if process.poll() is not None:
                    raise RuntimeError("application exited before validation completed")
                continue
            for address, values in messages(packet):
                if not values or not address.startswith("/pdj/channel/"):
                    continue
                parts = address.split("/")
                if len(parts) < 6:
                    continue
                channel = int(parts[3])
                if parts[4:6] == ["program", "moment"]:
                    moment = int(values[0])
                    if not moments or moments[-1] != moment:
                        moments.append(moment)
                        moment_times.append(time.monotonic() - started)
                elif parts[4:6] == ["program", "group_video_count"]:
                    group = channel // 4
                    occupancy[group] = int(values[0])
                    maximum_occupancy = max(maximum_occupancy, occupancy[group])
                elif parts[4:6] == ["generator", "content"]:
                    content[channel] = int(values[0])
            if moments and moments[-1] == 2 and len(content) == 8:
                groups = tuple(
                    tuple(ch for ch in range(group * 4, group * 4 + 4)
                          if content.get(ch) == 0)
                    for group in range(2)
                )
                if all(groups):
                    signature = groups
    finally:
        process.terminate()
        try:
            process.wait(timeout=4)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=2)
        receiver.close()

    # Los momentos del sistema unificado son sucesos que ocurren, no una apertura
    # fija, de modo que la ejecución puede empezar en cualquiera de ellos y revisitar uno
    # antes de estabilizarse en la intercalación.
    if not moments:
        raise AssertionError("no program moment was reported")
    if moments[0] not in (0, 1, 2):
        raise AssertionError(f"unexpected opening moment {moments[0]}")
    if 2 not in moments:
        raise AssertionError(f"intercalation was never reached: {moments}")
    for index, moment in enumerate(moments[:-1]):
        if moment == 2:
            raise AssertionError(f"intercalation was left again: {moments}")
        held = moment_times[index + 1] - moment_times[index]
        expected = pulse_seconds if moment == 0 else bar_seconds
        if held + 0.25 < expected:
            raise AssertionError(
                f"system moment {moment} dwell ended early ({held:.2f}s)"
            )
    if maximum_occupancy > 2:
        raise AssertionError(f"group video occupancy reached {maximum_occupancy}")
    if signature is None:
        raise AssertionError("intercalation video signature was not observed")
    return moments, signature


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--app",
        default="bin/Partitura_del_Juego.app/Contents/MacOS/Partitura_del_Juego"
    )
    parser.add_argument("--runs", type=int, default=2)
    parser.add_argument("--duration", type=float, default=15.0)
    parser.add_argument("--pulse-seconds", type=float, default=3.0)
    parser.add_argument("--bar-seconds", type=float, default=3.0)
    parser.add_argument("--port", type=int, default=19001)
    parser.add_argument("--seed", type=int, default=1346652721)
    args = parser.parse_args()
    app = Path(args.app).resolve()
    if not app.exists():
        raise SystemExit(f"application not found: {app}")

    results = [
        run_once(app, args.duration, args.pulse_seconds, args.bar_seconds,
                 args.port, args.seed)
        for _ in range(max(1, args.runs))
    ]
    if any(result != results[0] for result in results[1:]):
        raise AssertionError(f"seed repeatability failed: {results}")
    moments, signature = results[0]
    names = {0: "Pulse", 1: "BarScan", 2: "Intercalation"}
    order = " -> ".join(names[moment] for moment in moments)
    print(
        f"PASS: {order}; protected dwell; "
        f"video cap <= 2; repeatable signature {signature}"
    )


if __name__ == "__main__":
    main()
