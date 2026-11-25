"""Check that the packaged VST3 produces audio in a host at 44.1 kHz."""
import sys
from pathlib import Path

import numpy as np
from pedalboard import load_plugin


SAMPLE_RATE = 44_100
BLOCK_SIZE = 512


def check_audio(bundle: Path, lead_mode: bool = False) -> None:
    # Pedalboard's Windows scanner expects the module file inside the bundle.
    module = bundle / 'Contents' / 'x86_64-win' / bundle.name
    plugin = load_plugin(str(module.resolve()))
    if lead_mode:
        plugin.lead_channel = True

    silence = np.zeros((2, SAMPLE_RATE), dtype=np.float32)
    idle = plugin(silence, SAMPLE_RATE, buffer_size=BLOCK_SIZE)
    if not np.isfinite(idle).all() or np.max(np.abs(idle)) > 0.001:
        raise RuntimeError(f"{bundle.name} is producing unexpected output with no input")

    time = np.arange(SAMPLE_RATE * 2, dtype=np.float64) / SAMPLE_RATE
    tone = (10 ** (-18 / 20) * np.sin(2 * np.pi * 220 * time)).astype(np.float32)
    audio = np.stack((tone, tone))
    output = plugin(audio, SAMPLE_RATE, buffer_size=BLOCK_SIZE)
    if output.shape != audio.shape or not np.isfinite(output).all():
        raise RuntimeError(f"{bundle.name} returned invalid audio")

    peaks = np.max(np.abs(output[:, SAMPLE_RATE:]), axis=1)
    if np.any(peaks < 0.01) or np.any(peaks >= 1.0):
        raise RuntimeError(f"{bundle.name} returned an unexpected output level: {peaks}")
    mode = "lead" if lead_mode else "default"
    print(f"{bundle.name} ({mode}) peak output: {20 * np.log10(peaks)} dBFS (L/R)")


if __name__ == "__main__":
    vst3_bundle = Path(sys.argv[1])
    check_audio(vst3_bundle)
    if vst3_bundle.stem == "Lead85MKII":
        check_audio(vst3_bundle, lead_mode=True)
