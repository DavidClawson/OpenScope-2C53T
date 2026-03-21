# Signal Injector for OpenScope 2C53T Emulator
#
# Reads signal configuration from /tmp/openscope_signal.json and generates
# waveform data that can be fed to the firmware via the FPGA protocol.
#
# This is a stub for Phase 1 — actual FPGA sample injection requires
# understanding the sample transfer protocol over USART2.
#
# Configuration file format:
# {
#   "ch1_waveform": "sine",      // sine, square, triangle, sawtooth, noise
#   "ch1_frequency": 1000.0,     // Hz
#   "ch1_amplitude": 1.0,        // Vpp
#   "ch1_noise": 5,              // % of amplitude
#   "ch2_waveform": "square",
#   "ch2_frequency": 5000.0,
#   "ch2_amplitude": 0.5,
#   "ch2_noise": 0
# }

import json
import math
import os

SIGNAL_CONFIG_PATH = "/tmp/openscope_signal.json"
SAMPLE_RATE = 250000.0  # 250 kHz


def read_config():
    """Read signal configuration from shared file"""
    try:
        with open(SIGNAL_CONFIG_PATH, 'r') as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {
            "ch1_waveform": "sine", "ch1_frequency": 1000.0,
            "ch1_amplitude": 1.0, "ch1_noise": 5,
            "ch2_waveform": "square", "ch2_frequency": 5000.0,
            "ch2_amplitude": 0.5, "ch2_noise": 0,
        }


def generate_sample(waveform, phase):
    """Generate a single sample value (-1.0 to 1.0) at given phase (0.0 to 1.0)"""
    if waveform == "sine":
        return math.sin(2.0 * math.pi * phase)
    elif waveform == "square":
        return 1.0 if phase < 0.5 else -1.0
    elif waveform == "triangle":
        return 4.0 * abs(phase - 0.5) - 1.0
    elif waveform == "sawtooth":
        return 2.0 * phase - 1.0
    elif waveform == "noise":
        # Simple hash-based pseudorandom
        h = int(phase * 1000000) * 2654435761 & 0xFFFFFFFF
        return (h / 2147483648.0) - 1.0
    return 0.0


def generate_buffer(channel, num_samples, sample_offset=0):
    """Generate a buffer of int16 samples for a channel"""
    cfg = read_config()
    prefix = f"ch{channel}_"
    waveform = cfg.get(prefix + "waveform", "sine")
    frequency = cfg.get(prefix + "frequency", 1000.0)
    amplitude = cfg.get(prefix + "amplitude", 1.0)
    noise_pct = cfg.get(prefix + "noise", 0)

    scale = amplitude / 3.3 * 32767  # Scale to int16 range relative to 3.3V full scale
    samples = []

    for i in range(num_samples):
        t = (sample_offset + i) / SAMPLE_RATE
        phase = (t * frequency) % 1.0
        val = generate_sample(waveform, phase) * scale

        # Add noise
        if noise_pct > 0:
            noise_phase = ((sample_offset + i) * 7919) % 10000 / 10000.0
            noise_val = generate_sample("noise", noise_phase) * scale * noise_pct / 100.0
            val += noise_val

        samples.append(max(-32768, min(32767, int(val))))

    return samples
