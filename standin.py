"""Stand-in for the plugin. No audio.

Connects to a running Hoarder, sends hello and host, and checks that a
preview path is a WAV file.

    python standin.py
    python standin.py --bpm 126 --until-preview
"""

from __future__ import annotations

import argparse
import asyncio
import json
import uuid
from pathlib import Path

from websockets.asyncio.client import connect

FORMATS = frozenset({"vst3", "au", "stand-in"})
REPEATS = frozenset({"1/16", "1/8", "1/4", "1/2", "1bar", "2bar", "4bar"})


def check_preview(message: dict) -> Path:
    """Raise ValueError unless message points at a WAV on this machine."""
    if not isinstance(message, dict) or message.get("type") != "preview":
        raise ValueError("not a preview")
    raw = message.get("path")
    if not isinstance(raw, str) or not raw:
        raise ValueError("preview has no path")
    path = Path(raw)
    if path.suffix.lower() != ".wav":
        raise ValueError("preview is not a wav")
    if not path.is_file():
        raise ValueError("preview file is missing")
    with path.open("rb") as handle:
        head = handle.read(12)
    if len(head) < 12 or head[:4] != b"RIFF" or head[8:12] != b"WAVE":
        raise ValueError("preview is not a wav")
    loop = message.get("loop")
    beats = message.get("beats")
    repeat = message.get("repeat")
    if not isinstance(loop, bool):
        raise ValueError("preview loop flag is missing")
    if beats is not None and (isinstance(beats, bool) or not isinstance(beats, (int, float))):
        raise ValueError("preview beats are not a number")
    if repeat is not None and repeat not in REPEATS:
        raise ValueError("preview repeat rate is unknown")
    return path


def host_message(
    *,
    bpm: float,
    ppq: float,
    playing: bool,
    recording: bool,
    meter: tuple[int, int],
    sample_rate: float,
) -> dict:
    return {
        "type": "host",
        "bpm": bpm,
        "ppq": ppq,
        "playing": playing,
        "recording": recording,
        "meter": [meter[0], meter[1]],
        "sample_rate": sample_rate,
    }


async def run(args: argparse.Namespace) -> None:
    hello = {
        "type": "hello",
        "format": args.format,
        "host": args.host,
        "session": uuid.uuid4().hex,
    }
    print(f"hello {args.format} {args.host}", flush=True)
    async with connect(args.url, max_size=8192) as socket:
        await socket.send(json.dumps(hello))
        while True:
            host = host_message(
                bpm=args.bpm,
                ppq=args.ppq,
                playing=args.playing,
                recording=args.recording,
                meter=_meter(args.meter),
                sample_rate=args.sample_rate,
            )
            print(
                f"host bpm={host['bpm']:g} ppq={host['ppq']:g} "
                f"playing={str(host['playing']).lower()} recording={str(host['recording']).lower()}",
                flush=True,
            )
            await socket.send(json.dumps(host))
            try:
                raw = await asyncio.wait_for(socket.recv(), timeout=1 / args.hz)
            except asyncio.TimeoutError:
                if not args.freeze:
                    args.ppq += args.bpm / 60 / args.hz
                continue
            message = json.loads(raw)
            kind = message.get("type")
            if kind == "preview":
                path = check_preview(message)
                print(f"preview ok {path}", flush=True)
                if args.until_preview:
                    return
            elif kind == "stop":
                print("stop", flush=True)
            elif kind == "clear":
                print("clear", flush=True)
            else:
                print(f"ignored {kind}", flush=True)


def _meter(text: str) -> tuple[int, int]:
    num, den = text.split("/", 1)
    return int(num), int(den)


def main() -> None:
    parser = argparse.ArgumentParser(description="Talk to Hoarder on the link socket. No audio.")
    parser.add_argument("--url", default="ws://127.0.0.1:8771")
    parser.add_argument("--format", default="stand-in", choices=sorted(FORMATS))
    parser.add_argument("--host", default="stand-in")
    parser.add_argument("--bpm", type=float, default=120)
    parser.add_argument("--ppq", type=float, default=0)
    parser.add_argument("--playing", action="store_true")
    parser.add_argument("--recording", action="store_true")
    parser.add_argument("--meter", default="4/4")
    parser.add_argument("--sample-rate", type=float, default=44100)
    parser.add_argument("--hz", type=float, default=20)
    parser.add_argument("--freeze", action="store_true", help="keep ppq where it started")
    parser.add_argument("--until-preview", action="store_true")
    args = parser.parse_args()
    if args.hz <= 0:
        parser.error("--hz must be > 0")
    try:
        asyncio.run(run(args))
    except KeyboardInterrupt:
        return


if __name__ == "__main__":
    main()
