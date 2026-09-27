#!/usr/bin/env python3
"""Analyze a raw sample file the way tcp_record sees it.

Reports overall levels, the exact amplitude of a CTCSS tone (via DFT), its
per-second duty cycle, and — for 16-bit inputs — a byte-order sanity check,
since LE/BE confusion looks like quiet distorted audio.

Usage:
  tools/raw_stats.py FILE [-f FMT] [-r RATE] [--tone HZ]

Example:
  tools/raw_stats.py SR5W_16k_S16LE.raw -f s16le -r 16000 --tone 127.3
"""

import argparse
import math
import re
import sys

import numpy as np


def parse_args():
    p = argparse.ArgumentParser(description="Raw file / tone analyzer")
    p.add_argument("rawfile")
    p.add_argument("-f", "--format", default="s16le",
                   help="s8|u8|s16|u16|f32 with optional le/be (default: s16le)")
    p.add_argument("-r", "--rate", type=int, default=48000)
    p.add_argument("--tone", type=float, default=127.3,
                   help="CTCSS tone frequency to measure in Hz (0=off)")
    return p.parse_args()


def load(path, fmt):
    m = re.match(r"^(s8|u8|s16|u16|f32)(le|be)?$", fmt.lower())
    if not m:
        sys.exit("bad format %r" % fmt)
    kind, end = m.group(1), m.group(2) or "be"
    data = np.fromfile(path, np.uint8)
    if kind == "s8":
        return data.view(np.int8).astype(np.float64) / 128.0
    if kind == "u8":
        return (data.astype(np.float64) - 128.0) / 128.0
    dt = ("<" if end == "le" else ">") + ("i2" if kind == "s16" else "u2")
    raw = data.view(dt).astype(np.float64)
    if kind == "s16":
        return raw / 32768.0
    return (raw - 32768.0) / 32768.0


def dbfs(a):
    return 20.0 * math.log10(a + 1e-12)


def tone_amp(x, fs, hz):
    """Exact DFT amplitude of frequency hz over the whole signal."""
    n = len(x)
    k = np.exp(-2j * math.pi * hz * np.arange(n) / fs)
    return 2.0 * np.abs(x @ k) / n


def tone_per_sec(x, fs, hz):
    n = len(x) // fs
    t = np.arange(fs) / fs
    k = np.exp(-2j * math.pi * hz * t)
    amps = np.array([2.0 * abs(x[i * fs:(i + 1) * fs] @ k) / fs for i in range(n)])
    return 20.0 * np.log10(amps + 1e-12)


def intervals(on):
    """Contiguous True runs of a boolean array as (start, end) second pairs."""
    out = []
    start = None
    for i, v in enumerate(on):
        if v and start is None:
            start = i
        elif not v and start is not None:
            out.append((start, i))
            start = None
    if start is not None:
        out.append((start, len(on)))
    return out


def analyze(x, fs, tone):
    print("samples %d (%.1fs), dc %.5f, rms %.1f dBFS, peak %.1f dBFS"
          % (len(x), len(x) / fs, x.mean(), dbfs(x.std()), dbfs(np.abs(x).max())))

    if tone > 0:
        a = tone_amp(x, fs, tone)
        print("tone %.1f Hz whole-file amplitude: %.1f dBFS" % (tone, dbfs(a)))
        db = tone_per_sec(x, fs, tone)
        for th in (-30, -35, -40, -45, -55, -65):
            print("  per-sec tone > %3d dBFS: %3d/%d s (%.0f%%)"
                  % (th, (db > th).sum(), len(db), 100 * (db > th).mean()))
        print("  per-sec tone percentiles 5/50/95: %s dBFS"
              % np.percentile(db, [5, 50, 95]).round(1))
        print("  tone-on intervals (> -45 dBFS):")
        for s, e in intervals(db > -45):
            if e - s >= 1:
                print("    %3ds - %3ds  (%ds)" % (s, e, e - s))

    # dominant spectral peak between 20 Hz and 4 kHz (1-s hann windows)
    seg = min(fs, 8192)
    w = np.hanning(seg)
    freqs = np.fft.rfftfreq(seg, 1 / fs)
    psd = np.zeros(len(freqs))
    for i in range(0, len(x) - seg, seg):
        psd += np.abs(np.fft.rfft(x[i:i + seg] * w)) ** 2
    band = (freqs >= 20) & (freqs <= 4000)
    pk = freqs[band][np.argmax(psd[band])]
    print("dominant spectral peak 20-4000 Hz: %.1f Hz" % pk)


def main():
    o = parse_args()
    x = load(o.rawfile, o.format)
    print("== %s as %s, %d Hz ==" % (o.rawfile, o.format, o.rate))
    analyze(x, o.rate, o.tone)
    if o.format.lower().startswith(("s16", "u16")):
        other = o.format[:-2] + ("be" if o.format.endswith("le") else "le")
        y = load(o.rawfile, other)
        print("-- byte-order sanity: same file as %s --" % other)
        analyze(y, o.rate, 0)
        if o.tone > 0:
            print("  tone %.1f Hz as %s: %.1f dBFS"
                  % (o.tone, other, dbfs(tone_amp(y, o.rate, o.tone))))


if __name__ == "__main__":
    main()
