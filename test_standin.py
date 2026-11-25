"""The stand-in accepts a real WAV and rejects anything else."""

from pathlib import Path

import pytest

from standin import check_preview


def test_check_preview_accepts_a_wav(tmp_path: Path) -> None:
    path = tmp_path / "kick.wav"
    path.write_bytes(b"RIFF" + b"\x00\x00\x00\x00" + b"WAVE" + b"rest")
    found = check_preview(
        {"type": "preview", "path": str(path), "loop": False, "beats": 1, "repeat": None}
    )
    assert found == path


def test_check_preview_rejects_a_missing_or_non_wav(tmp_path: Path) -> None:
    other = tmp_path / "kick.aif"
    other.write_bytes(b"FORM")
    with pytest.raises(ValueError):
        check_preview({"type": "preview", "path": str(other), "loop": False, "beats": None, "repeat": None})
    wav = tmp_path / "not.wav"
    wav.write_bytes(b"not-a-wav-file!!")
    with pytest.raises(ValueError):
        check_preview({"type": "preview", "path": str(wav), "loop": True, "beats": None, "repeat": "1/16"})
